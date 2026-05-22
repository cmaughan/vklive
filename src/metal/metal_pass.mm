#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <vector>

#include <fmt/format.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <vklive/camera.h>
#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_model.h>
#include <vklive/metal/metal_nanovg.h>
#include <vklive/metal/metal_pass.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_shader.h>
#include <vklive/python_scripting.h>
#include <vklive/scene.h>
#include <vklive/validation.h>

namespace
{

constexpr size_t kMetalMaxColorAttachments = 8;

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

void release_obj(void*& object)
{
    if (object)
    {
        id releasedObject = CFBridgingRelease(object);
        releasedObject = nil;
        object = nullptr;
    }
}

void retain_obj(void*& storage, id object)
{
    release_obj(storage);
    storage = object ? (__bridge_retained void*)object : nullptr;
}

NSString* ns_string(const std::string& string)
{
    return [NSString stringWithUTF8String:string.c_str()];
}

std::string ns_string(NSString* string)
{
    return string ? std::string([string UTF8String]) : std::string();
}

void report_pass_error(metal::MetalPass& pass, const std::string& text, const fs::path& path = fs::path(), int32_t line = -1)
{
    scene_report_error(pass.pass.scene, MessageSeverity::Error, text, path.empty() ? pass.pass.scene.sceneGraphPath : path, line);
    validation_error(text);
}

struct BasicPassShaders
{
    metal::MetalShader* vertex = nullptr;
    metal::MetalShader* fragment = nullptr;
};

const ShaderBindingMeta* metal_pass_find_binding_meta(const metal::MetalShader& shader, uint32_t set, uint32_t binding)
{
    auto itrSet = shader.bindingSets.find(set);
    if (itrSet == shader.bindingSets.end())
    {
        return nullptr;
    }

    auto itrMeta = itrSet->second.bindingMeta.find(binding);
    if (itrMeta == itrSet->second.bindingMeta.end())
    {
        return nullptr;
    }

    return &itrMeta->second;
}

bool metal_pass_binding_is_bound_by_basic_pass(const metal::MetalShaderResourceBinding& binding)
{
    return binding.set == 0 && binding.binding == 0 && binding.type == ShaderBindingType::UniformBuffer && binding.count == 1;
}

bool metal_pass_binding_is_sampled_surface(const metal::MetalShaderResourceBinding& binding)
{
    return binding.type == ShaderBindingType::CombinedImageSampler || binding.type == ShaderBindingType::SampledImage || binding.type == ShaderBindingType::Sampler;
}

bool metal_pass_binding_requires_texture(const metal::MetalShaderResourceBinding& binding)
{
    return binding.type == ShaderBindingType::CombinedImageSampler || binding.type == ShaderBindingType::SampledImage;
}

bool metal_pass_binding_requires_sampler(const metal::MetalShaderResourceBinding& binding)
{
    return binding.type == ShaderBindingType::CombinedImageSampler || binding.type == ShaderBindingType::Sampler;
}

bool metal_pass_binding_is_material_resource(const metal::MetalShaderResourceBinding& binding, const std::string& name)
{
    if (binding.set != 2)
    {
        return false;
    }

    if (binding.binding == 0 || name == "vklMaterials" || name == "VklMaterials")
    {
        return binding.type == ShaderBindingType::StorageBuffer && binding.count == 1;
    }

    if (name == "vklBaseColorTextures" ||
        name == "vklNormalTextures" ||
        name == "vklMetallicRoughnessTextures" ||
        name == "vklEmissiveTextures" ||
        name == "vklOcclusionTextures")
    {
        return binding.type == ShaderBindingType::CombinedImageSampler && binding.count <= metal::MetalMaxMaterials;
    }

    return false;
}

using MetalSampledSurfaces = std::map<std::string, metal::MetalSurface*>;

MetalSampledSurfaces metal_pass_sampled_surfaces(metal::MetalContext& ctx, metal::MetalPass& pass)
{
    MetalSampledSurfaces sampledSurfaces;
    for (auto& passSampler : pass.pass.samplers)
    {
        auto* surface = metal::metal_scene_get_or_create_surface(ctx, pass.metalScene, passSampler.sampler, Scene::GlobalFrameCount, passSampler.sampleAlternate);
        if (surface)
        {
            sampledSurfaces[passSampler.sampler] = surface;
        }
    }
    return sampledSurfaces;
}

bool metal_pass_validate_shader_bindings(metal::MetalPass& pass, const metal::MetalShader& shader, const MetalSampledSurfaces& sampledSurfaces)
{
    for (const auto& [_, binding] : shader.resourceBindings)
    {
        if (metal_pass_binding_is_bound_by_basic_pass(binding))
        {
            continue;
        }

        const auto* meta = metal_pass_find_binding_meta(shader, binding.set, binding.binding);
        const auto name = meta ? meta->name : std::string("<unnamed>");
        const auto path = meta ? meta->shaderPath : (shader.pShader ? shader.pShader->path : fs::path());
        const auto line = meta ? meta->line : -1;

        if (meta && metal_pass_binding_is_material_resource(binding, meta->name))
        {
            continue;
        }

        if (binding.set == 2)
        {
            report_pass_error(pass,
                fmt::format("Metal pass '{}' cannot bind reflected material resource '{}' at set {}, binding {} ({}, count {}). Expected vklMaterials or one of the vkl material texture arrays.",
                    pass.pass.name,
                    name,
                    binding.set,
                    binding.binding,
                    shader_binding_type_to_string(binding.type),
                    binding.count),
                path,
                line);
            return false;
        }

        if (metal_pass_binding_is_sampled_surface(binding))
        {
            if (binding.count != 1)
            {
                report_pass_error(pass,
                    fmt::format("Metal pass '{}' cannot bind sampled shader resource '{}' at set {}, binding {} ({}, count {}). Metal sampled surfaces support a single descriptor per binding.",
                        pass.pass.name,
                        name,
                        binding.set,
                        binding.binding,
                        shader_binding_type_to_string(binding.type),
                        binding.count),
                    path,
                    line);
                return false;
            }

            if (!meta || meta->name.empty())
            {
                report_pass_error(pass,
                    fmt::format("Metal pass '{}' cannot bind sampled shader resource at set {}, binding {} ({}, count {}) because reflection did not provide a resource name.",
                        pass.pass.name,
                        binding.set,
                        binding.binding,
                        shader_binding_type_to_string(binding.type),
                        binding.count),
                    path,
                    line);
                return false;
            }

            auto itrSurface = sampledSurfaces.find(meta->name);
            if (itrSurface == sampledSurfaces.end() || !itrSurface->second)
            {
                report_pass_error(pass,
                    fmt::format("Metal pass '{}' cannot bind sampled shader resource '{}' at set {}, binding {} ({}, count {}) because the pass does not declare a sampler with that name.",
                        pass.pass.name,
                        meta->name,
                        binding.set,
                        binding.binding,
                        shader_binding_type_to_string(binding.type),
                        binding.count),
                    path,
                    line);
                return false;
            }

            auto* surface = itrSurface->second;
            if (surface->allocationState == metal::MetalAllocationState::Failed)
            {
                report_pass_error(pass,
                    fmt::format("Metal pass '{}' cannot bind sampled shader resource '{}' at set {}, binding {} ({}, count {}) because surface loading failed.",
                        pass.pass.name,
                        meta->name,
                        binding.set,
                        binding.binding,
                        shader_binding_type_to_string(binding.type),
                        binding.count),
                    path,
                    line);
                return false;
            }

            if ((metal_pass_binding_requires_texture(binding) && !surface->texture) || (metal_pass_binding_requires_sampler(binding) && !surface->sampler))
            {
                report_pass_error(pass,
                    fmt::format("Metal pass '{}' cannot bind sampled shader resource '{}' at set {}, binding {} ({}, count {}) because the prepared surface is missing {}{}.",
                        pass.pass.name,
                        meta->name,
                        binding.set,
                        binding.binding,
                        shader_binding_type_to_string(binding.type),
                        binding.count,
                        metal_pass_binding_requires_texture(binding) && !surface->texture ? "texture" : "",
                        metal_pass_binding_requires_texture(binding) && !surface->texture && metal_pass_binding_requires_sampler(binding) && !surface->sampler ? " and sampler" : (metal_pass_binding_requires_sampler(binding) && !surface->sampler ? "sampler" : "")),
                    path,
                    line);
                return false;
            }

            continue;
        }

        report_pass_error(pass,
            fmt::format("Metal pass '{}' cannot bind reflected shader resource '{}' at set {}, binding {} ({}, count {}). Metal raster passes currently support the single default UBO, named sampled surfaces, and set 2 model material resources.",
                pass.pass.name,
                name,
                binding.set,
                binding.binding,
                shader_binding_type_to_string(binding.type),
                binding.count),
            path,
            line);
        return false;
    }

    return true;
}

bool metal_pass_get_shaders(metal::MetalPass& pass, BasicPassShaders& shaders)
{
    uint32_t vertexCount = 0;
    uint32_t fragmentCount = 0;

    for (const auto& shaderPath : pass.pass.shaders)
    {
        auto itrShader = pass.metalScene.shaderStages.find(shaderPath);
        if (itrShader == pass.metalScene.shaderStages.end() || !itrShader->second)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' is missing compiled shader stage '{}'.", pass.pass.name, shaderPath.filename().string()), shaderPath);
            return false;
        }

        auto& shader = *itrShader->second;
        if (shader.stage == metal::MetalShaderStage::Vertex)
        {
            shaders.vertex = &shader;
            vertexCount++;
        }
        else if (shader.stage == metal::MetalShaderStage::Fragment)
        {
            shaders.fragment = &shader;
            fragmentCount++;
        }
    }

    if (vertexCount != 1 || fragmentCount != 1)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' requires exactly one vertex shader and one fragment shader for basic raster rendering (found {} vertex, {} fragment).", pass.pass.name, vertexCount, fragmentCount));
        return false;
    }

    if (!shaders.vertex || !shaders.vertex->function || !shaders.fragment || !shaders.fragment->function)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' has a shader stage without a Metal function.", pass.pass.name));
        return false;
    }

    return true;
}

MTLPixelFormat metal_pixel_format(metal::MetalSurfaceFormat format)
{
    switch (format)
    {
    case metal::MetalSurfaceFormat::Depth32Float:
        return MTLPixelFormatDepth32Float;
    case metal::MetalSurfaceFormat::RGBA16Float:
        return MTLPixelFormatRGBA16Float;
    case metal::MetalSurfaceFormat::RGBA32Float:
        return MTLPixelFormatRGBA32Float;
    case metal::MetalSurfaceFormat::RGBA8Unorm:
        return MTLPixelFormatRGBA8Unorm;
    case metal::MetalSurfaceFormat::RGBA8Unorm_sRGB:
        return MTLPixelFormatRGBA8Unorm_sRGB;
    default:
        return MTLPixelFormatInvalid;
    }
}

bool metal_format_is_depth(metal::MetalSurfaceFormat format)
{
    return format == metal::MetalSurfaceFormat::Depth32Float;
}

MTLVertexFormat metal_vertex_format(VertexComponent component)
{
    switch (component)
    {
    case VERTEX_COMPONENT_UV:
        return MTLVertexFormatFloat2;
    case VERTEX_COMPONENT_COLOR:
        return MTLVertexFormatFloat4;
    case VERTEX_COMPONENT_DUMMY_FLOAT:
        return MTLVertexFormatFloat;
    case VERTEX_COMPONENT_DUMMY_INT:
        return MTLVertexFormatInt;
    case VERTEX_COMPONENT_DUMMY_VEC4:
        return MTLVertexFormatFloat4;
    case VERTEX_COMPONENT_DUMMY_INT4:
        return MTLVertexFormatInt4;
    case VERTEX_COMPONENT_DUMMY_UINT4:
        return MTLVertexFormatUInt4;
    case VERTEX_COMPONENT_POSITION:
    case VERTEX_COMPONENT_NORMAL:
    case VERTEX_COMPONENT_TANGENT:
    case VERTEX_COMPONENT_BITANGENT:
    default:
        return MTLVertexFormatFloat3;
    }
}

MTLVertexDescriptor* metal_vertex_descriptor(const VertexLayout& layout)
{
    MTLVertexDescriptor* descriptor = [MTLVertexDescriptor vertexDescriptor];
    descriptor.layouts[0].stride = layout_size(layout);
    descriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    descriptor.layouts[0].stepRate = 1;

    for (uint32_t i = 0; i < layout.components.size(); ++i)
    {
        descriptor.attributes[i].format = metal_vertex_format(layout.components[i]);
        descriptor.attributes[i].offset = layout_offset(layout, i);
        descriptor.attributes[i].bufferIndex = 0;
    }

    return descriptor;
}

bool metal_pass_prepare_targets(metal::MetalContext& ctx, metal::MetalPass& pass, const glm::uvec2& renderSize, metal::MetalPassTargets& targets)
{
    auto passTargets = pass.pass.targets;
    if (passTargets.empty())
    {
        passTargets.push_back("default_color");
        passTargets.push_back("default_depth");
    }

    for (const auto& targetName : passTargets)
    {
        auto pMetalSurface = metal::metal_scene_get_or_create_surface(ctx, pass.metalScene, targetName, Scene::GlobalFrameCount);
        if (!pMetalSurface)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' could not find target '{}'.", pass.pass.name, targetName));
            return false;
        }

        if (!pMetalSurface->pSurface || !pMetalSurface->pSurface->isTarget)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' target '{}' is not a render target.", pass.pass.name, targetName));
            return false;
        }

        if (!metal::metal_surface_ensure_target(ctx, pass.metalScene, *pMetalSurface, renderSize))
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' could not create target '{}'.", pass.pass.name, targetName));
            return false;
        }

        const auto pixelFormat = metal_pixel_format(pMetalSurface->format);
        if (pixelFormat == MTLPixelFormatInvalid || !pMetalSurface->texture)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' target '{}' has an unsupported Metal pixel format.", pass.pass.name, targetName));
            return false;
        }

        if (metal_format_is_depth(pMetalSurface->format))
        {
            if (targets.depth)
            {
                report_pass_error(pass, fmt::format("Metal pass '{}' has more than one depth target. Basic Metal raster rendering supports only one optional depth target.", pass.pass.name));
                return false;
            }
            targets.depth = pMetalSurface;
            targets.depthFormat = static_cast<uint32_t>(pixelFormat);
        }
        else
        {
            if (targets.colors.empty())
            {
                targets.size = pMetalSurface->size;
            }
            else if (pMetalSurface->size != targets.size)
            {
                const auto firstColorName = targets.colors.front() && targets.colors.front()->pSurface ? targets.colors.front()->pSurface->name : std::string("<unknown>");
                report_pass_error(pass,
                    fmt::format("Metal pass '{}' target '{}' size {}x{} does not match first color target '{}' size {}x{}.",
                        pass.pass.name,
                        targetName,
                        pMetalSurface->size.x,
                        pMetalSurface->size.y,
                        firstColorName,
                        targets.size.x,
                        targets.size.y));
                return false;
            }
            targets.colors.push_back(pMetalSurface);
            targets.colorFormats.push_back(static_cast<uint32_t>(pixelFormat));
        }
    }

    if (targets.colors.empty())
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' has no color target. Basic Metal raster rendering requires one color target.", pass.pass.name));
        return false;
    }

    if (targets.depth && targets.depth->size != targets.size)
    {
        const auto firstColorName = targets.colors.front() && targets.colors.front()->pSurface ? targets.colors.front()->pSurface->name : std::string("<unknown>");
        const auto depthName = targets.depth->pSurface ? targets.depth->pSurface->name : std::string("<unknown>");
        report_pass_error(pass,
            fmt::format("Metal pass '{}' depth target '{}' size {}x{} does not match first color target '{}' size {}x{}.",
                pass.pass.name,
                depthName,
                targets.depth->size.x,
                targets.depth->size.y,
                firstColorName,
                targets.size.x,
                targets.size.y));
        return false;
    }

    pass.targetSize = targets.size;
    pass.colorTargetKeys.clear();
    pass.colorTargetGenerations.clear();
    pass.colorTargetKeys.reserve(targets.colors.size());
    pass.colorTargetGenerations.reserve(targets.colors.size());
    for (const auto* color : targets.colors)
    {
        pass.colorTargetKeys.push_back(color->key);
        pass.colorTargetGenerations.push_back(color->generation);
    }
    pass.depthTargetKey = targets.depth ? targets.depth->key : metal::MetalSurfaceKey();
    pass.depthTargetGeneration = targets.depth ? targets.depth->generation : 0;
    return true;
}

bool metal_pass_prepare_samplers(metal::MetalContext& ctx, metal::MetalPass& pass)
{
    bool ok = true;
    for (auto& passSampler : pass.pass.samplers)
    {
        if (passSampler.sampleAlternate)
        {
            report_pass_error(pass,
                fmt::format("Metal sampled surface feedback/ping-pong is not implemented yet; sampler '!{}' / surface {} cannot be used yet.",
                    passSampler.sampler,
                    passSampler.sampler),
                pass.pass.scene.sceneGraphPath,
                pass.pass.scriptSamplersLine);
            ok = false;
            continue;
        }

        auto* metalSurface = metal::metal_scene_get_or_create_surface(ctx, pass.metalScene, passSampler.sampler, Scene::GlobalFrameCount, passSampler.sampleAlternate);
        if (!metalSurface || !metalSurface->pSurface)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' could not find sampled surface '{}'.", pass.pass.name, passSampler.sampler), pass.pass.scene.sceneGraphPath, pass.pass.scriptSamplersLine);
            ok = false;
            continue;
        }

        auto& surface = *metalSurface->pSurface;
        if (!surface.isTarget)
        {
            if (surface.name == "AudioAnalysis")
            {
                if (!metal::metal_surface_update_from_audio(ctx, *metalSurface))
                {
                    report_pass_error(pass, fmt::format("Metal pass '{}' could not prepare audio sampled surface '{}'.", pass.pass.name, passSampler.sampler), pass.pass.scene.sceneGraphPath, pass.pass.scriptSamplersLine);
                    ok = false;
                }
            }
            else if (metalSurface->allocationState == metal::MetalAllocationState::Init)
            {
                if (surface.path.empty())
                {
                    report_pass_error(pass, fmt::format("Metal pass '{}' sampled surface '{}' has no texture path.", pass.pass.name, passSampler.sampler), pass.pass.scene.sceneGraphPath, pass.pass.scriptSamplersLine);
                    metalSurface->allocationState = metal::MetalAllocationState::Failed;
                    ok = false;
                }
                else
                {
                    auto file = scene_find_asset(pass.pass.scene, surface.path, AssetType::Texture);
                    if (file.empty())
                    {
                        report_pass_error(pass, fmt::format("Metal pass '{}' could not find texture '{}' for sampled surface '{}'.", pass.pass.name, surface.path.string(), passSampler.sampler), pass.pass.scene.sceneGraphPath, pass.pass.scriptSamplersLine);
                        metalSurface->allocationState = metal::MetalAllocationState::Failed;
                        ok = false;
                    }
                    else if (!metal::metal_surface_create_from_file(ctx, *metalSurface, file))
                    {
                        report_pass_error(pass, fmt::format("Metal pass '{}' could not load texture '{}' for sampled surface '{}'.", pass.pass.name, file.string(), passSampler.sampler), file);
                        metalSurface->allocationState = metal::MetalAllocationState::Failed;
                        ok = false;
                    }
                }
            }
        }

        if (!metalSurface->sampler && metalSurface->texture)
        {
            metal::metal_surface_create_sampler(ctx, *metalSurface);
        }
    }
    return ok;
}

void metal_pass_reset_pipeline(metal::MetalPass& pass)
{
    release_obj(pass.renderPipelineState);
    release_obj(pass.computePipelineState);
    release_obj(pass.depthStencilState);
    pass.colorPixelFormats.clear();
    pass.depthPixelFormat = 0;
}

bool metal_pass_ensure_pipeline(metal::MetalContext& ctx, metal::MetalPass& pass, const BasicPassShaders& shaders, const metal::MetalPassTargets& targets)
{
    if (targets.colorFormats.empty())
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot create a pipeline without at least one color attachment.", pass.pass.name));
        return false;
    }

    if (targets.colorFormats.size() > kMetalMaxColorAttachments)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' has {} color attachments, but Metal supports at most {}.", pass.pass.name, targets.colorFormats.size(), kMetalMaxColorAttachments));
        return false;
    }

    std::vector<uint32_t> colorPixelFormats;
    colorPixelFormats.reserve(targets.colorFormats.size());
    for (const auto colorFormat : targets.colorFormats)
    {
        colorPixelFormats.push_back(colorFormat);
    }

    auto depthPixelFormat = targets.depthFormat;
    if (pass.renderPipelineState && pass.colorPixelFormats == colorPixelFormats && pass.depthPixelFormat == depthPixelFormat)
    {
        return true;
    }

    metal_pass_reset_pipeline(pass);

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot create a pipeline because the Metal device is unavailable.", pass.pass.name));
        return false;
    }

    MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.label = ns_string(fmt::format("MetalPass:{}", pass.pass.name));
    descriptor.vertexFunction = bridge<id<MTLFunction>>(shaders.vertex->function);
    descriptor.fragmentFunction = bridge<id<MTLFunction>>(shaders.fragment->function);
    descriptor.vertexDescriptor = metal_vertex_descriptor(g_vertexLayout);
    descriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
    descriptor.rasterSampleCount = 1;
    for (NSUInteger i = 0; i < targets.colorFormats.size(); ++i)
    {
        descriptor.colorAttachments[i].pixelFormat = static_cast<MTLPixelFormat>(targets.colorFormats[i]);
        descriptor.colorAttachments[i].blendingEnabled = YES;
        descriptor.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        descriptor.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        descriptor.colorAttachments[i].rgbBlendOperation = MTLBlendOperationAdd;
        descriptor.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorOne;
        descriptor.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        descriptor.colorAttachments[i].alphaBlendOperation = MTLBlendOperationAdd;
    }

    if (targets.depth)
    {
        descriptor.depthAttachmentPixelFormat = static_cast<MTLPixelFormat>(targets.depthFormat);
    }

    NSError* error = nil;
    id<MTLRenderPipelineState> pipelineState = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!pipelineState)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not create render pipeline: {}", pass.pass.name, ns_string([error localizedDescription])));
        return false;
    }
    retain_obj(pass.renderPipelineState, pipelineState);

    if (targets.depth)
    {
        MTLDepthStencilDescriptor* depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
        depthDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual;
        depthDescriptor.depthWriteEnabled = YES;
        id<MTLDepthStencilState> depthState = [device newDepthStencilStateWithDescriptor:depthDescriptor];
        if (!depthState)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' could not create depth stencil state.", pass.pass.name));
            return false;
        }
        retain_obj(pass.depthStencilState, depthState);
    }

    pass.colorPixelFormats = colorPixelFormats;
    pass.depthPixelFormat = depthPixelFormat;
    return true;
}

metal::MetalShader* metal_pass_get_ray_shader(metal::MetalPass& pass)
{
    if (pass.pass.metalRayKernel.empty())
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' is a ray tracing pass but has no native Metal ray kernel.", pass.pass.name));
        return nullptr;
    }

    auto itrShader = pass.metalScene.shaderStages.find(pass.pass.metalRayKernel);
    if (itrShader == pass.metalScene.shaderStages.end() || !itrShader->second)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' is missing compiled native ray kernel '{}'.", pass.pass.name, pass.pass.metalRayKernel.filename().string()), pass.pass.metalRayKernel);
        return nullptr;
    }

    auto& shader = *itrShader->second;
    if (shader.stage != metal::MetalShaderStage::RayCompute || !shader.function)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' native ray kernel '{}' did not compile to a compute function.", pass.pass.name, pass.pass.metalRayKernel.filename().string()), pass.pass.metalRayKernel);
        return nullptr;
    }
    return &shader;
}

bool metal_pass_ensure_ray_pipeline(metal::MetalContext& ctx, metal::MetalPass& pass, const metal::MetalShader& shader)
{
    if (pass.computePipelineState)
    {
        return true;
    }

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot create a compute pipeline because the Metal device is unavailable.", pass.pass.name));
        return false;
    }

    NSError* error = nil;
    id<MTLComputePipelineState> pipelineState = [device newComputePipelineStateWithFunction:bridge<id<MTLFunction>>(shader.function) error:&error];
    if (!pipelineState)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not create native ray compute pipeline: {}", pass.pass.name, ns_string([error localizedDescription])), shader.path);
        return false;
    }

    retain_obj(pass.computePipelineState, pipelineState);
    return true;
}

metal::MetalModel* metal_pass_find_ray_model(metal::MetalPass& pass)
{
    for (const auto& modelPath : pass.pass.models)
    {
        auto itrModel = pass.metalScene.models.find(modelPath);
        if (itrModel == pass.metalScene.models.end() || !itrModel->second)
        {
            continue;
        }

        auto& model = *itrModel->second;
        if (model.accelerationStructuresBuilt && model.topLevelAccelerationStructure)
        {
            return &model;
        }
    }
    return nullptr;
}

bool metal_pass_ray_tracing_supported(metal::MetalContext& ctx, metal::MetalPass& pass)
{
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot dispatch ray tracing because the Metal device is unavailable.", pass.pass.name));
        return false;
    }

    if (@available(macOS 11.0, *))
    {
        if (![device supportsRaytracing])
        {
            report_pass_error(pass, fmt::format("Metal ray tracing is unsupported on this macOS/Metal device; pass '{}' requires a device where supportsRaytracing is true.", pass.pass.name));
            return false;
        }
        return true;
    }

    report_pass_error(pass, fmt::format("Metal ray tracing is unsupported on this macOS version; pass '{}' requires macOS 11.0 or newer with Metal ray tracing support.", pass.pass.name));
    return false;
}

struct MetalPassChannel
{
    alignas(16) glm::vec4 resolution = glm::vec4(0.0f);
    alignas(4) float time = 0.0f;
};

struct MetalPassUBO
{
    alignas(4) float iTime = 0.0f;
    alignas(4) float iGlobalTime = 0.0f;
    alignas(4) float iTimeDelta = 0.0f;
    alignas(4) float iFrame = 0.0f;
    alignas(4) float iFrameRate = 0.0f;
    alignas(4) float iSampleRate = 0.0f;
    alignas(4) uint32_t iSceneFlags = 0;
    alignas(4) uint32_t vertexSize = 0;
    alignas(16) glm::vec4 iResolution = glm::vec4(0.0f);
    alignas(16) glm::vec4 iMouse = glm::vec4(0.0f);
    alignas(16) glm::vec4 iDate = glm::vec4(0.0f);
    alignas(16) glm::vec4 iSpectrumBands[2] = {};
    alignas(16) glm::vec4 iChannelTime = glm::vec4(0.0f);
    alignas(16) glm::vec4 iChannelResolution[4] = {};
    alignas(16) glm::vec4 ifFragCoordOffsetUniform = glm::vec4(0.0f);
    alignas(16) glm::vec4 eye = glm::vec4(0.0f);
    alignas(16) glm::mat4 model = glm::mat4(1.0f);
    alignas(16) glm::mat4 view = glm::mat4(1.0f);
    alignas(16) glm::mat4 projection = glm::mat4(1.0f);
    alignas(16) glm::mat4 modelViewProjection = glm::mat4(1.0f);
    alignas(16) glm::mat4 viewInverse = glm::mat4(1.0f);
    alignas(16) glm::mat4 projectionInverse = glm::mat4(1.0f);
    MetalPassChannel iChannel[4];
};

bool metal_pass_update_uniforms(metal::MetalContext& ctx, metal::MetalPass& pass, const metal::MetalPassTargets& targets)
{
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot create uniforms because the Metal device is unavailable.", pass.pass.name));
        return false;
    }

    auto uniformBuffer = bridge<id<MTLBuffer>>(pass.uniformBuffer);
    if (!uniformBuffer)
    {
        uniformBuffer = [device newBufferWithLength:sizeof(MetalPassUBO) options:MTLResourceStorageModeShared];
        if (!uniformBuffer)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' could not create uniform buffer.", pass.pass.name));
            return false;
        }
        uniformBuffer.label = ns_string(fmt::format("MetalPass:{}:Uniforms", pass.pass.name));
        retain_obj(pass.uniformBuffer, uniformBuffer);
    }

    MetalPassUBO ubo;
    auto& scene = pass.pass.scene;
    auto size = targets.size;

    ubo.model = glm::mat4(1.0f);
    for (const auto& cameraName : pass.pass.cameras)
    {
        auto itrCamera = scene.cameras.find(cameraName);
        if (itrCamera == scene.cameras.end() || !itrCamera->second)
        {
            continue;
        }

        auto& camera = *itrCamera->second;
        camera_set_film_size(camera, glm::ivec2(size));
        camera_pre_render(camera);

        ubo.view = camera_get_lookat(camera);
        ubo.projection = camera_get_projection(camera);
        ubo.modelViewProjection = ubo.projection * ubo.view * ubo.model;
        ubo.viewInverse = glm::inverse(ubo.view);
        ubo.projectionInverse = glm::inverse(ubo.projection);
        ubo.eye = glm::vec4(camera.position, 0.0f);
    }

    const auto elapsed = static_cast<float>(Scene::GlobalElapsedSeconds);
    ubo.iTimeDelta = pass.lastUniformTime == 0.0f ? 0.0f : elapsed - pass.lastUniformTime;
    ubo.iTime = elapsed;
    ubo.iGlobalTime = elapsed;
    ubo.iFrame = static_cast<float>(Scene::GlobalFrameCount);
    ubo.iFrameRate = elapsed != 0.0f ? (1.0f / elapsed) : 0.0f;
    ubo.iSceneFlags = scene.sceneFlags;
    ubo.vertexSize = layout_size(g_vertexLayout);
    ubo.iResolution = glm::vec4(size.x, size.y, 1.0f, 0.0f);

    for (uint32_t i = 0; i < 4; ++i)
    {
        ubo.iChannelResolution[i] = ubo.iResolution;
        ubo.iChannelTime[i] = ubo.iTime;
        ubo.iChannel[i].resolution = ubo.iResolution;
        ubo.iChannel[i].time = ubo.iTime;
    }

    std::memcpy([uniformBuffer contents], &ubo, sizeof(ubo));
    [uniformBuffer didModifyRange:NSMakeRange(0, sizeof(ubo))];
    pass.lastUniformTime = elapsed;
    return true;
}

void metal_pass_set_buffer(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, const metal::MetalShaderResourceBinding& binding, id<MTLBuffer> buffer)
{
    if (!buffer || binding.bufferIndex == std::numeric_limits<uint32_t>::max())
    {
        return;
    }
    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        [encoder setVertexBuffer:buffer offset:0 atIndex:binding.bufferIndex];
    }
    else
    {
        [encoder setFragmentBuffer:buffer offset:0 atIndex:binding.bufferIndex];
    }
}

void metal_pass_set_texture(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, const metal::MetalShaderResourceBinding& binding, id<MTLTexture> texture)
{
    if (!texture || binding.textureIndex == std::numeric_limits<uint32_t>::max())
    {
        return;
    }
    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        [encoder setVertexTexture:texture atIndex:binding.textureIndex];
    }
    else
    {
        [encoder setFragmentTexture:texture atIndex:binding.textureIndex];
    }
}

void metal_pass_set_sampler(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, const metal::MetalShaderResourceBinding& binding, id<MTLSamplerState> sampler)
{
    if (!sampler || binding.samplerIndex == std::numeric_limits<uint32_t>::max())
    {
        return;
    }
    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        [encoder setVertexSamplerState:sampler atIndex:binding.samplerIndex];
    }
    else
    {
        [encoder setFragmentSamplerState:sampler atIndex:binding.samplerIndex];
    }
}

void metal_pass_bind_uniform(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, id<MTLBuffer> uniformBuffer)
{
    auto itrBinding = shader.resourceBindings.find({ 0, 0 });
    if (itrBinding == shader.resourceBindings.end() || itrBinding->second.type != ShaderBindingType::UniformBuffer || itrBinding->second.count != 1)
    {
        return;
    }
    metal_pass_set_buffer(encoder, shader, itrBinding->second, uniformBuffer);
}

std::optional<uint32_t> metal_pass_find_buffer_argument_index(const metal::MetalShader& shader, const std::string& argumentName)
{
    size_t searchOffset = 0;
    while (true)
    {
        const auto namePos = shader.mslSource.find(argumentName, searchOffset);
        if (namePos == std::string::npos)
        {
            return std::nullopt;
        }

        const auto bufferPos = shader.mslSource.find("[[buffer(", namePos);
        if (bufferPos == std::string::npos)
        {
            return std::nullopt;
        }

        const auto nextComma = shader.mslSource.find(',', namePos);
        const auto nextCloseParen = shader.mslSource.find(')', namePos);
        const auto argumentEnd = std::min(nextComma == std::string::npos ? shader.mslSource.size() : nextComma,
            nextCloseParen == std::string::npos ? shader.mslSource.size() : nextCloseParen);
        if (bufferPos < argumentEnd)
        {
            const auto digitStart = bufferPos + std::string("[[buffer(").size();
            uint32_t value = 0;
            bool sawDigit = false;
            for (size_t i = digitStart; i < shader.mslSource.size(); ++i)
            {
                const char c = shader.mslSource[i];
                if (c < '0' || c > '9')
                {
                    break;
                }
                sawDigit = true;
                value = (value * 10) + static_cast<uint32_t>(c - '0');
            }
            if (sawDigit)
            {
                return value;
            }
        }

        searchOffset = namePos + argumentName.size();
    }
}

bool metal_pass_set_draw_push_constants(id<MTLRenderCommandEncoder> encoder, metal::MetalPass& pass, const metal::MetalShader& shader, const metal::MetalDrawPushConstants& constants)
{
    auto bufferIndex = metal_pass_find_buffer_argument_index(shader, "vklDraw");
    if (!bufferIndex)
    {
        return true;
    }

    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        if (*bufferIndex == 0)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' cannot bind vertex draw push constants at buffer(0) because vertex buffer slot 0 is reserved for model vertices.", pass.pass.name), shader.pShader ? shader.pShader->path : fs::path());
            return false;
        }
        [encoder setVertexBytes:&constants length:sizeof(constants) atIndex:*bufferIndex];
    }
    else
    {
        [encoder setFragmentBytes:&constants length:sizeof(constants) atIndex:*bufferIndex];
    }
    return true;
}

void metal_pass_set_argument_texture_array(id<MTLRenderCommandEncoder> encoder, MTLRenderStages stages, id<MTLArgumentEncoder> argumentEncoder, const metal::MetalShaderResourceBinding& binding, const std::vector<metal::MetalModelTexture*>& textures)
{
    if (binding.textureIndex == std::numeric_limits<uint32_t>::max() || binding.count == 0 || textures.empty())
    {
        return;
    }

    const auto count = std::min<size_t>(binding.count, textures.size());
    for (size_t i = 0; i < count; ++i)
    {
        auto texture = textures[i] ? bridge<id<MTLTexture>>(textures[i]->texture) : nil;
        [argumentEncoder setTexture:texture atIndex:binding.textureIndex + i];
        if (texture)
        {
            [encoder useResource:texture usage:MTLResourceUsageRead stages:stages];
        }
    }
}

void metal_pass_set_argument_sampler_array(id<MTLArgumentEncoder> argumentEncoder, const metal::MetalShaderResourceBinding& binding, const std::vector<metal::MetalModelTexture*>& textures)
{
    if (binding.samplerIndex == std::numeric_limits<uint32_t>::max() || binding.count == 0 || textures.empty())
    {
        return;
    }

    const auto count = std::min<size_t>(binding.count, textures.size());
    for (size_t i = 0; i < count; ++i)
    {
        [argumentEncoder setSamplerState:textures[i] ? bridge<id<MTLSamplerState>>(textures[i]->sampler) : nil atIndex:binding.samplerIndex + i];
    }
}

bool metal_pass_bind_material_argument_buffer(metal::MetalContext& ctx, id<MTLRenderCommandEncoder> encoder, NSMutableArray* liveArgumentBuffers, metal::MetalPass& pass, const metal::MetalShader& shader, metal::MetalModel& model)
{
    bool hasMaterialResources = false;
    for (const auto& [_, binding] : shader.resourceBindings)
    {
        const auto* meta = metal_pass_find_binding_meta(shader, binding.set, binding.binding);
        if (meta && metal_pass_binding_is_material_resource(binding, meta->name))
        {
            hasMaterialResources = true;
            break;
        }
    }

    if (!hasMaterialResources)
    {
        return true;
    }

    const auto argumentBufferIndex = metal_pass_find_buffer_argument_index(shader, "spvDescriptorSet2");
    if (!argumentBufferIndex)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not find the set 2 material argument buffer in shader '{}'.", pass.pass.name, shader.pShader ? shader.pShader->path.filename().string() : std::string("<unknown>")), shader.pShader ? shader.pShader->path : fs::path());
        return false;
    }

    if (shader.stage == metal::MetalShaderStage::Vertex && *argumentBufferIndex == 0)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot bind vertex material argument buffer at buffer(0) because vertex buffer slot 0 is reserved for model vertices.", pass.pass.name), shader.pShader ? shader.pShader->path : fs::path());
        return false;
    }

    auto function = bridge<id<MTLFunction>>(shader.function);
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!function || !device)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot create material argument buffer because the Metal function or device is unavailable.", pass.pass.name), shader.pShader ? shader.pShader->path : fs::path());
        return false;
    }

    id<MTLArgumentEncoder> argumentEncoder = [function newArgumentEncoderWithBufferIndex:*argumentBufferIndex];
    if (!argumentEncoder)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not create material argument encoder for shader '{}'.", pass.pass.name, shader.pShader ? shader.pShader->path.filename().string() : std::string("<unknown>")), shader.pShader ? shader.pShader->path : fs::path());
        return false;
    }

    id<MTLBuffer> argumentBuffer = [device newBufferWithLength:argumentEncoder.encodedLength options:MTLResourceStorageModeShared];
    if (!argumentBuffer)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not allocate material argument buffer for model '{}'.", pass.pass.name, model.debugName), shader.pShader ? shader.pShader->path : fs::path());
        return false;
    }
    argumentBuffer.label = ns_string(fmt::format("{}:{}:MaterialArguments", pass.pass.name, model.debugName));
    [argumentEncoder setArgumentBuffer:argumentBuffer offset:0];
    const auto resourceStages = shader.stage == metal::MetalShaderStage::Vertex ? MTLRenderStageVertex : MTLRenderStageFragment;

    for (const auto& [_, binding] : shader.resourceBindings)
    {
        const auto* meta = metal_pass_find_binding_meta(shader, binding.set, binding.binding);
        if (!meta || !metal_pass_binding_is_material_resource(binding, meta->name))
        {
            continue;
        }

        if (binding.binding == 0 || meta->name == "vklMaterials" || meta->name == "VklMaterials")
        {
            auto materialsBuffer = bridge<id<MTLBuffer>>(model.materialsBuffer);
            if (!materialsBuffer)
            {
                report_pass_error(pass, fmt::format("Metal pass '{}' model '{}' has no material buffer.", pass.pass.name, model.debugName), meta->shaderPath, meta->line);
                return false;
            }
            [argumentEncoder setBuffer:materialsBuffer offset:0 atIndex:binding.bufferIndex];
            [encoder useResource:materialsBuffer usage:MTLResourceUsageRead stages:resourceStages];
        }
        else if (meta->name == "vklBaseColorTextures")
        {
            metal_pass_set_argument_texture_array(encoder, resourceStages, argumentEncoder, binding, model.baseColorTextures);
            metal_pass_set_argument_sampler_array(argumentEncoder, binding, model.baseColorTextures);
        }
        else if (meta->name == "vklNormalTextures")
        {
            metal_pass_set_argument_texture_array(encoder, resourceStages, argumentEncoder, binding, model.normalTextures);
            metal_pass_set_argument_sampler_array(argumentEncoder, binding, model.normalTextures);
        }
        else if (meta->name == "vklMetallicRoughnessTextures")
        {
            metal_pass_set_argument_texture_array(encoder, resourceStages, argumentEncoder, binding, model.metallicRoughnessTextures);
            metal_pass_set_argument_sampler_array(argumentEncoder, binding, model.metallicRoughnessTextures);
        }
        else if (meta->name == "vklEmissiveTextures")
        {
            metal_pass_set_argument_texture_array(encoder, resourceStages, argumentEncoder, binding, model.emissiveTextures);
            metal_pass_set_argument_sampler_array(argumentEncoder, binding, model.emissiveTextures);
        }
        else if (meta->name == "vklOcclusionTextures")
        {
            metal_pass_set_argument_texture_array(encoder, resourceStages, argumentEncoder, binding, model.occlusionTextures);
            metal_pass_set_argument_sampler_array(argumentEncoder, binding, model.occlusionTextures);
        }
    }

    [liveArgumentBuffers addObject:argumentBuffer];
    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        [encoder setVertexBuffer:argumentBuffer offset:0 atIndex:*argumentBufferIndex];
    }
    else
    {
        [encoder setFragmentBuffer:argumentBuffer offset:0 atIndex:*argumentBufferIndex];
    }
    return true;
}

bool metal_pass_encode_draw(metal::MetalContext& ctx, metal::MetalPass& pass, const BasicPassShaders& shaders, const metal::MetalPassTargets& targets)
{
    auto sampledSurfaces = metal_pass_sampled_surfaces(ctx, pass);

    auto commandQueue = bridge<id<MTLCommandQueue>>(ctx.commandQueue);
    if (!commandQueue)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot draw because the command queue is unavailable.", pass.pass.name));
        return false;
    }

    MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
    for (NSUInteger i = 0; i < targets.colors.size(); ++i)
    {
        renderPass.colorAttachments[i].texture = bridge<id<MTLTexture>>(targets.colors[i]->texture);
        renderPass.colorAttachments[i].loadAction = pass.pass.hasClear ? MTLLoadActionClear : MTLLoadActionLoad;
        renderPass.colorAttachments[i].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[i].clearColor = MTLClearColorMake(pass.pass.clearColor.x, pass.pass.clearColor.y, pass.pass.clearColor.z, pass.pass.clearColor.w);
    }

    if (targets.depth)
    {
        renderPass.depthAttachment.texture = bridge<id<MTLTexture>>(targets.depth->texture);
        renderPass.depthAttachment.loadAction = MTLLoadActionClear;
        renderPass.depthAttachment.storeAction = MTLStoreActionStore;
        renderPass.depthAttachment.clearDepth = 1.0;
    }

    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    commandBuffer.label = ns_string(fmt::format("MetalPass:{}", pass.pass.name));

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];
    if (!encoder)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not create a render command encoder.", pass.pass.name));
        return false;
    }

    encoder.label = ns_string(fmt::format("MetalPass:{}:Encoder", pass.pass.name));
    [encoder setRenderPipelineState:bridge<id<MTLRenderPipelineState>>(pass.renderPipelineState)];
    if (pass.depthStencilState)
    {
        [encoder setDepthStencilState:bridge<id<MTLDepthStencilState>>(pass.depthStencilState)];
    }
    [encoder setCullMode:MTLCullModeBack];
    [encoder setFrontFacingWinding:MTLWindingClockwise];

    MTLViewport viewport{ 0.0, 0.0, static_cast<double>(targets.size.x), static_cast<double>(targets.size.y), 0.0, 1.0 };
    MTLScissorRect scissor{ 0, 0, static_cast<NSUInteger>(targets.size.x), static_cast<NSUInteger>(targets.size.y) };
    [encoder setViewport:viewport];
    [encoder setScissorRect:scissor];

    auto uniformBuffer = bridge<id<MTLBuffer>>(pass.uniformBuffer);
    if (uniformBuffer)
    {
        metal_pass_bind_uniform(encoder, *shaders.vertex, uniformBuffer);
        metal_pass_bind_uniform(encoder, *shaders.fragment, uniformBuffer);
    }

    auto bindSampledSurfaces = [&](const metal::MetalShader& shader) {
        for (const auto& [_, metalBinding] : shader.resourceBindings)
        {
            if (!metal_pass_binding_is_sampled_surface(metalBinding))
            {
                continue;
            }

            const auto* meta = metal_pass_find_binding_meta(shader, metalBinding.set, metalBinding.binding);
            if (!meta)
            {
                continue;
            }

            auto itrSurface = sampledSurfaces.find(meta->name);
            if (itrSurface == sampledSurfaces.end() || !itrSurface->second)
            {
                continue;
            }

            auto* surface = itrSurface->second;
            metal_pass_set_texture(encoder, shader, metalBinding, bridge<id<MTLTexture>>(surface->texture));
            metal_pass_set_sampler(encoder, shader, metalBinding, bridge<id<MTLSamplerState>>(surface->sampler));
        }
    };

    bindSampledSurfaces(*shaders.vertex);
    bindSampledSurfaces(*shaders.fragment);

    NSMutableArray* liveArgumentBuffers = [NSMutableArray array];
    for (const auto& modelPath : pass.pass.models)
    {
        auto itrModel = pass.metalScene.models.find(modelPath);
        if (itrModel == pass.metalScene.models.end() || !itrModel->second)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' could not find model '{}'.", pass.pass.name, modelPath.string()), modelPath);
            [encoder endEncoding];
            return false;
        }

        auto& model = *itrModel->second;
        auto vertexBuffer = bridge<id<MTLBuffer>>(model.vertexBuffer);
        auto indexBuffer = bridge<id<MTLBuffer>>(model.indexBuffer);
        if (!model.staged || !vertexBuffer || !indexBuffer)
        {
            report_pass_error(pass, fmt::format("Metal pass '{}' model '{}' is not staged.", pass.pass.name, modelPath.string()), modelPath);
            [encoder endEncoding];
            return false;
        }

        [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
        if (!metal_pass_bind_material_argument_buffer(ctx, encoder, liveArgumentBuffers, pass, *shaders.vertex, model) ||
            !metal_pass_bind_material_argument_buffer(ctx, encoder, liveArgumentBuffers, pass, *shaders.fragment, model))
        {
            [encoder endEncoding];
            return false;
        }

        if (!model.parts.empty())
        {
            for (const auto& part : model.parts)
            {
                metal::MetalDrawPushConstants constants;
                constants.materialIndex = part.materialIndex;
                if (!metal_pass_set_draw_push_constants(encoder, pass, *shaders.vertex, constants) ||
                    !metal_pass_set_draw_push_constants(encoder, pass, *shaders.fragment, constants))
                {
                    [encoder endEncoding];
                    return false;
                }

                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:part.indexCount
                                     indexType:MTLIndexTypeUInt32
                                   indexBuffer:indexBuffer
                             indexBufferOffset:part.indexBase * sizeof(uint32_t)];
            }
        }
        else if (model.indexCount > 0)
        {
            metal::MetalDrawPushConstants constants;
            constants.materialIndex = 0;
            if (!metal_pass_set_draw_push_constants(encoder, pass, *shaders.vertex, constants) ||
                !metal_pass_set_draw_push_constants(encoder, pass, *shaders.fragment, constants))
            {
                [encoder endEncoding];
                return false;
            }

            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:model.indexCount
                                 indexType:MTLIndexTypeUInt32
                               indexBuffer:indexBuffer
                         indexBufferOffset:0];
        }
    }

    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    if (commandBuffer.status == MTLCommandBufferStatusError)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' command buffer failed: {}", pass.pass.name, ns_string([commandBuffer.error localizedDescription])));
        return false;
    }

    for (auto* color : targets.colors)
    {
        if (color && color->pSurface)
        {
            color->pSurface->rendered = true;
        }
    }

    return true;
}

bool metal_pass_encode_ray_trace(metal::MetalContext& ctx, metal::MetalPass& pass, const metal::MetalShader& shader, const metal::MetalPassTargets& targets)
{
    if (!metal_pass_ray_tracing_supported(ctx, pass))
    {
        return false;
    }

    auto* model = metal_pass_find_ray_model(pass);
    if (!model)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' requires at least one model with built Metal acceleration structures.", pass.pass.name));
        return false;
    }

    if (targets.colors.empty() || !targets.colors.front() || !targets.colors.front()->texture)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' requires a writable color target for native ray tracing.", pass.pass.name));
        return false;
    }

    auto commandQueue = bridge<id<MTLCommandQueue>>(ctx.commandQueue);
    if (!commandQueue)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot dispatch ray tracing because the command queue is unavailable.", pass.pass.name));
        return false;
    }

    auto pipelineState = bridge<id<MTLComputePipelineState>>(pass.computePipelineState);
    auto outputTexture = bridge<id<MTLTexture>>(targets.colors.front()->texture);
    auto uniformBuffer = bridge<id<MTLBuffer>>(pass.uniformBuffer);
    auto vertexBuffer = bridge<id<MTLBuffer>>(model->vertexBuffer);
    auto indexBuffer = bridge<id<MTLBuffer>>(model->indexBuffer);
    if (!pipelineState || !outputTexture || !uniformBuffer || !vertexBuffer || !indexBuffer)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot dispatch ray tracing because required pipeline, target, uniform, vertex, or index resources are missing.", pass.pass.name));
        return false;
    }

    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    if (!commandBuffer)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not allocate a ray tracing command buffer.", pass.pass.name));
        return false;
    }
    commandBuffer.label = ns_string(fmt::format("MetalPass:{}:RayTrace", pass.pass.name));

    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    if (!encoder)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' could not create a ray tracing compute encoder.", pass.pass.name));
        return false;
    }
    encoder.label = ns_string(fmt::format("MetalPass:{}:RayTraceEncoder", pass.pass.name));

    [encoder setComputePipelineState:pipelineState];
    [encoder setTexture:outputTexture atIndex:0];
    [encoder setBuffer:uniformBuffer offset:0 atIndex:1];
    [encoder setBuffer:vertexBuffer offset:0 atIndex:2];
    [encoder setBuffer:indexBuffer offset:0 atIndex:3];
    [encoder useResource:outputTexture usage:MTLResourceUsageWrite];
    [encoder useResource:uniformBuffer usage:MTLResourceUsageRead];
    [encoder useResource:vertexBuffer usage:MTLResourceUsageRead];
    [encoder useResource:indexBuffer usage:MTLResourceUsageRead];

    if (@available(macOS 11.0, *))
    {
        auto tlas = bridge<id<MTLAccelerationStructure>>(model->topLevelAccelerationStructure);
        auto blas = bridge<id<MTLAccelerationStructure>>(model->bottomLevelAccelerationStructure);
        if (!tlas)
        {
            [encoder endEncoding];
            report_pass_error(pass, fmt::format("Metal pass '{}' model '{}' has no top-level acceleration structure.", pass.pass.name, model->debugName));
            return false;
        }
        [encoder setAccelerationStructure:tlas atBufferIndex:0];
        [encoder useResource:(id<MTLResource>)tlas usage:MTLResourceUsageRead];
        if (blas)
        {
            [encoder useResource:(id<MTLResource>)blas usage:MTLResourceUsageRead];
        }
    }
    else
    {
        [encoder endEncoding];
        report_pass_error(pass, fmt::format("Metal ray tracing is unsupported on this macOS version; pass '{}' requires macOS 11.0 or newer with Metal ray tracing support.", pass.pass.name));
        return false;
    }

    const MTLSize gridSize = MTLSizeMake(targets.size.x, targets.size.y, 1);
    const MTLSize threadsPerThreadgroup = MTLSizeMake(8, 8, 1);
    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadsPerThreadgroup];
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    if (commandBuffer.status == MTLCommandBufferStatusError)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' ray tracing command buffer failed: {}", pass.pass.name, ns_string([commandBuffer.error localizedDescription])));
        return false;
    }

    for (auto* color : targets.colors)
    {
        if (color && color->pSurface)
        {
            color->pSurface->rendered = true;
        }
    }

    return true;
}

bool metal_pass_supported(metal::MetalPass& pass)
{
    if (pass.pass.passType == PassType::Scripted)
    {
        return true;
    }

    if (pass.pass.passType == PassType::RayTracing)
    {
        return true;
    }

    if (pass.pass.passType != PassType::Standard)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' has unsupported pass type. Metal rendering supports standard raster, native ray tracing, and scripted passes.", pass.pass.name));
        pass.reportedUnsupportedFeatures = true;
        return false;
    }

    if (pass.pass.models.empty())
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' has no models to draw.", pass.pass.name));
        pass.reportedUnsupportedFeatures = true;
        return false;
    }

    return true;
}

} // namespace

namespace metal
{

std::shared_ptr<MetalPass> metal_pass_create(MetalScene& scene, Pass& pass)
{
    return std::make_shared<MetalPass>(scene, pass);
}

void metal_pass_destroy(MetalContext& ctx, MetalPass& pass)
{
    (void)ctx;
    metal_pass_reset_pipeline(pass);
    release_obj(pass.uniformBuffer);
    pass.targetSize = glm::uvec2(0);
    pass.colorTargetKeys.clear();
    pass.depthTargetKey = MetalSurfaceKey();
    pass.colorTargetGenerations.clear();
    pass.depthTargetGeneration = 0;
    pass.lastUniformTime = 0.0f;
}

bool metal_pass_draw(MetalContext& ctx, MetalPass& pass, const glm::uvec2& renderSize)
{
    validation_set_shaders(pass.pass.shaders);

    if (!metal_pass_supported(pass))
    {
        validation_set_shaders({});
        return false;
    }

    if (pass.pass.passType == PassType::Scripted)
    {
        MetalPassTargets targets;
        const bool ok = metal_pass_prepare_targets(ctx, pass, renderSize, targets) &&
            metal_nanovg_begin(ctx, pass, targets) &&
            ([&]() {
                python_run_pass(ctx.vg, pass.pass, targets.size);
                return true;
            })() &&
            metal_nanovg_end(ctx, pass);

        if (ok)
        {
            for (auto* color : targets.colors)
            {
                if (color && color->pSurface)
                {
                    color->pSurface->rendered = true;
                }
            }
        }

        validation_set_shaders({});
        return ok && !validation_get_error_state() && pass.pass.scene.valid;
    }

    if (pass.pass.passType == PassType::RayTracing)
    {
        MetalPassTargets targets;
        metal::MetalShader* rayShader = nullptr;
        const bool ok = metal_pass_prepare_targets(ctx, pass, renderSize, targets) &&
            metal_pass_prepare_samplers(ctx, pass) &&
            metal_pass_update_uniforms(ctx, pass, targets) &&
            metal_pass_ray_tracing_supported(ctx, pass) &&
            ([&]() {
                rayShader = metal_pass_get_ray_shader(pass);
                return rayShader != nullptr;
            })() &&
            metal_pass_ensure_ray_pipeline(ctx, pass, *rayShader) &&
            metal_pass_encode_ray_trace(ctx, pass, *rayShader, targets);

        validation_set_shaders({});
        return ok && !validation_get_error_state() && pass.pass.scene.valid;
    }

    BasicPassShaders shaders;
    MetalPassTargets targets;
    MetalSampledSurfaces sampledSurfaces;
    const bool ok = metal_pass_get_shaders(pass, shaders) &&
        metal_pass_prepare_samplers(ctx, pass) &&
        ([&]() {
            sampledSurfaces = metal_pass_sampled_surfaces(ctx, pass);
            return metal_pass_validate_shader_bindings(pass, *shaders.vertex, sampledSurfaces) &&
                metal_pass_validate_shader_bindings(pass, *shaders.fragment, sampledSurfaces);
        })() &&
        metal_pass_prepare_targets(ctx, pass, renderSize, targets) &&
        metal_pass_ensure_pipeline(ctx, pass, shaders, targets) &&
        metal_pass_update_uniforms(ctx, pass, targets) &&
        metal_pass_encode_draw(ctx, pass, shaders, targets);

    validation_set_shaders({});
    return ok && !validation_get_error_state() && pass.pass.scene.valid;
}

} // namespace metal
