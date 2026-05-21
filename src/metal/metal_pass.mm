#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstring>
#include <limits>
#include <map>

#include <fmt/format.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <vklive/camera.h>
#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_model.h>
#include <vklive/metal/metal_pass.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_shader.h>
#include <vklive/scene.h>
#include <vklive/validation.h>

namespace
{

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
            fmt::format("Metal pass '{}' cannot bind reflected shader resource '{}' at set {}, binding {} ({}, count {}). Metal raster passes currently support the single default UBO and named sampled surfaces only.",
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

struct MetalPassTargets
{
    metal::MetalSurface* color = nullptr;
    metal::MetalSurface* depth = nullptr;
    glm::uvec2 size = glm::uvec2(0);
    MTLPixelFormat colorFormat = MTLPixelFormatInvalid;
    MTLPixelFormat depthFormat = MTLPixelFormatInvalid;
};

bool metal_pass_prepare_targets(metal::MetalContext& ctx, metal::MetalPass& pass, const glm::uvec2& renderSize, MetalPassTargets& targets)
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
            targets.depthFormat = pixelFormat;
        }
        else
        {
            if (targets.color)
            {
                report_pass_error(pass, fmt::format("Metal pass '{}' has more than one color target. Basic Metal raster rendering supports one color target.", pass.pass.name));
                return false;
            }
            targets.color = pMetalSurface;
            targets.colorFormat = pixelFormat;
        }
    }

    if (!targets.color)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' has no color target. Basic Metal raster rendering requires one color target.", pass.pass.name));
        return false;
    }

    targets.size = targets.color->size;
    if (targets.depth && targets.depth->size != targets.size)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' target sizes do not match: color {}x{}, depth {}x{}.",
                                   pass.pass.name,
                                   targets.size.x,
                                   targets.size.y,
                                   targets.depth->size.x,
                                   targets.depth->size.y));
        return false;
    }

    pass.targetSize = targets.size;
    pass.colorTargetKey = targets.color->key;
    pass.depthTargetKey = targets.depth ? targets.depth->key : metal::MetalSurfaceKey();
    pass.colorTargetGeneration = targets.color->generation;
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
    release_obj(pass.depthStencilState);
    pass.colorPixelFormat = 0;
    pass.depthPixelFormat = 0;
}

bool metal_pass_ensure_pipeline(metal::MetalContext& ctx, metal::MetalPass& pass, const BasicPassShaders& shaders, const MetalPassTargets& targets)
{
    auto colorPixelFormat = static_cast<uint32_t>(targets.colorFormat);
    auto depthPixelFormat = static_cast<uint32_t>(targets.depthFormat);
    if (pass.renderPipelineState && pass.colorPixelFormat == colorPixelFormat && pass.depthPixelFormat == depthPixelFormat)
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
    descriptor.colorAttachments[0].pixelFormat = targets.colorFormat;
    descriptor.colorAttachments[0].blendingEnabled = YES;
    descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    if (targets.depth)
    {
        descriptor.depthAttachmentPixelFormat = targets.depthFormat;
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

    pass.colorPixelFormat = colorPixelFormat;
    pass.depthPixelFormat = depthPixelFormat;
    return true;
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

bool metal_pass_update_uniforms(metal::MetalContext& ctx, metal::MetalPass& pass, const MetalPassTargets& targets)
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

bool metal_pass_encode_draw(metal::MetalContext& ctx, metal::MetalPass& pass, const BasicPassShaders& shaders, const MetalPassTargets& targets)
{
    auto sampledSurfaces = metal_pass_sampled_surfaces(ctx, pass);

    auto commandQueue = bridge<id<MTLCommandQueue>>(ctx.commandQueue);
    if (!commandQueue)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' cannot draw because the command queue is unavailable.", pass.pass.name));
        return false;
    }

    MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPass.colorAttachments[0].texture = bridge<id<MTLTexture>>(targets.color->texture);
    renderPass.colorAttachments[0].loadAction = pass.pass.hasClear ? MTLLoadActionClear : MTLLoadActionLoad;
    renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    renderPass.colorAttachments[0].clearColor = MTLClearColorMake(pass.pass.clearColor.x, pass.pass.clearColor.y, pass.pass.clearColor.z, pass.pass.clearColor.w);

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

        if (!model.parts.empty())
        {
            for (const auto& part : model.parts)
            {
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:part.indexCount
                                     indexType:MTLIndexTypeUInt32
                                   indexBuffer:indexBuffer
                             indexBufferOffset:part.indexBase * sizeof(uint32_t)];
            }
        }
        else if (model.indexCount > 0)
        {
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

    if (targets.color && targets.color->pSurface)
    {
        targets.color->pSurface->rendered = true;
    }

    return true;
}

bool metal_pass_supported(metal::MetalPass& pass)
{
    if (pass.pass.passType != PassType::Standard)
    {
        report_pass_error(pass, fmt::format("Metal pass '{}' has unsupported pass type. Basic Metal raster rendering only supports standard passes.", pass.pass.name));
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
    pass.colorTargetKey = MetalSurfaceKey();
    pass.depthTargetKey = MetalSurfaceKey();
    pass.colorTargetGeneration = 0;
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
