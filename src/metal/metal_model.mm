#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <vklive/metal/metal_model.h>

#include <array>
#include <utility>

#include <fmt/format.h>

#include <zest/file/runtree.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_model_as.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_surface.h>
#include <vklive/scene.h>

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

void report_model_error(metal::MetalScene& scene, const std::string& text, const fs::path& path = fs::path())
{
    if (scene.pScene)
    {
        scene_report_error(*scene.pScene, MessageSeverity::Error, text, path);
    }
}

fs::path resolve_model_path(metal::MetalScene& scene, const Geometry& geom)
{
    if (geom.type == GeometryType::Model)
    {
        if (geom.path.empty())
        {
            report_model_error(scene, "Could not load model: empty model path.");
        }
        return geom.path;
    }

    if (geom.type == GeometryType::Rect)
    {
        auto loadPath = Zest::runtree_find_path("models/quad.obj");
        if (loadPath.empty())
        {
            report_model_error(scene, fmt::format("Could not load default asset: {}", "runtree/models/quad.obj"));
        }
        return loadPath;
    }

    report_model_error(scene, "Could not load model: unsupported geometry type.");
    return {};
}

void metal_model_release_texture(metal::MetalModelTexture& texture)
{
    release_obj(texture.texture);
    release_obj(texture.sampler);
    texture.sourcePath.clear();
    texture.fallback = true;
}

metal::MetalModelTexture* metal_model_store_texture(metal::MetalModel& model, metal::MetalModelTexture&& texture)
{
    model.materialTextures.push_back(std::move(texture));
    return &model.materialTextures.back();
}

bool metal_model_create_sampler(metal::MetalContext& ctx, metal::MetalModelTexture& texture, const std::string& debugName)
{
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        return false;
    }

    MTLSamplerDescriptor* descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.minFilter = MTLSamplerMinMagFilterLinear;
    descriptor.magFilter = MTLSamplerMinMagFilterLinear;
    descriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
    descriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.label = ns_string(debugName + ":Sampler");

    id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:descriptor];
    if (!sampler)
    {
        return false;
    }

    retain_obj(texture.sampler, sampler);
    return true;
}

bool metal_model_create_solid_texture(metal::MetalContext& ctx, metal::MetalModelTexture& texture, const std::string& debugName, const std::array<uint8_t, 4>& color, MTLPixelFormat pixelFormat)
{
    metal_model_release_texture(texture);

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        return false;
    }

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat width:1 height:1 mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageShaderRead;

    id<MTLTexture> mtlTexture = [device newTextureWithDescriptor:descriptor];
    if (!mtlTexture)
    {
        return false;
    }

    mtlTexture.label = ns_string(debugName);
    MTLRegion region = MTLRegionMake2D(0, 0, 1, 1);
    [mtlTexture replaceRegion:region mipmapLevel:0 withBytes:color.data() bytesPerRow:color.size()];

    retain_obj(texture.texture, mtlTexture);
    texture.sourcePath = debugName;
    texture.fallback = true;
    return metal_model_create_sampler(ctx, texture, debugName);
}

metal::MetalModelTexture* metal_model_load_texture_slot(metal::MetalContext& ctx,
    metal::MetalModel& model,
    const ModelTextureSlot& slot,
    metal::MetalModelTexture& fallback,
    const std::string& debugPrefix)
{
    metal::MetalSurface surface(nullptr);
    surface.debugName = debugPrefix;
    const auto ldrFormat = slot.srgb ? metal::MetalSurfaceFormat::RGBA8Unorm_sRGB : metal::MetalSurfaceFormat::RGBA8Unorm;

    bool loaded = false;
    fs::path sourcePath;
    if (slot.embeddedTexture && !slot.embeddedTexture->data.empty())
    {
        sourcePath = slot.pathName;
        surface.debugName = fmt::format("{}:{}", debugPrefix, slot.pathName);
        loaded = metal::metal_surface_create_from_memory(ctx,
            surface,
            fs::path(slot.pathName),
            reinterpret_cast<const char*>(slot.embeddedTexture->data.data()),
            slot.embeddedTexture->data.size(),
            slot.flipY,
            ldrFormat);
    }
    else if (!slot.resolvedPath.empty())
    {
        sourcePath = slot.resolvedPath;
        surface.debugName = fmt::format("{}:{}", debugPrefix, slot.resolvedPath.filename().string());
        loaded = metal::metal_surface_create_from_file(ctx, surface, slot.resolvedPath, slot.flipY, ldrFormat);
    }

    if (!loaded || !surface.texture || !surface.sampler)
    {
        metal::metal_surface_destroy(ctx, surface);
        return &fallback;
    }

    metal::MetalModelTexture texture;
    texture.sourcePath = sourcePath;
    texture.fallback = false;
    retain_obj(texture.texture, bridge<id<MTLTexture>>(surface.texture));
    retain_obj(texture.sampler, bridge<id<MTLSamplerState>>(surface.sampler));
    metal::metal_surface_destroy(ctx, surface);
    return metal_model_store_texture(model, std::move(texture));
}

bool metal_model_prepare_materials(metal::MetalContext& ctx, metal::MetalScene& scene, metal::MetalModel& model)
{
    if (model.materialsBuffer || model.vertexData.empty())
    {
        return true;
    }

    if (!metal_model_create_solid_texture(ctx, model.fallbackBaseColorTexture, fmt::format("{}:FallbackBaseColor", model.debugName), { 255, 255, 255, 255 }, MTLPixelFormatRGBA8Unorm_sRGB) ||
        !metal_model_create_solid_texture(ctx, model.fallbackNormalTexture, fmt::format("{}:FallbackNormal", model.debugName), { 128, 128, 255, 255 }, MTLPixelFormatRGBA8Unorm) ||
        !metal_model_create_solid_texture(ctx, model.fallbackMetallicRoughnessTexture, fmt::format("{}:FallbackMetallicRoughness", model.debugName), { 0, 255, 0, 255 }, MTLPixelFormatRGBA8Unorm) ||
        !metal_model_create_solid_texture(ctx, model.fallbackEmissiveTexture, fmt::format("{}:FallbackEmissive", model.debugName), { 0, 0, 0, 255 }, MTLPixelFormatRGBA8Unorm_sRGB) ||
        !metal_model_create_solid_texture(ctx, model.fallbackOcclusionTexture, fmt::format("{}:FallbackOcclusion", model.debugName), { 255, 255, 255, 255 }, MTLPixelFormatRGBA8Unorm))
    {
        report_model_error(scene, fmt::format("Could not create fallback material textures for model: {}", model.createInfo.filename));
        return false;
    }

    model.materialTextures.reserve(model.materials.size() * 5);
    model.baseColorTextures.assign(metal::MetalMaxMaterials, &model.fallbackBaseColorTexture);
    model.normalTextures.assign(metal::MetalMaxMaterials, &model.fallbackNormalTexture);
    model.metallicRoughnessTextures.assign(metal::MetalMaxMaterials, &model.fallbackMetallicRoughnessTexture);
    model.emissiveTextures.assign(metal::MetalMaxMaterials, &model.fallbackEmissiveTexture);
    model.occlusionTextures.assign(metal::MetalMaxMaterials, &model.fallbackOcclusionTexture);

    model.gpuMaterials.resize(metal::MetalMaxMaterials);
    for (uint32_t i = 0; i < metal::MetalMaxMaterials; ++i)
    {
        metal::MetalGpuMaterial gpuMaterial;
        if (i < model.materials.size())
        {
            const auto& material = model.materials[i];
            gpuMaterial.baseColorFactor = material.baseColorFactor;
            gpuMaterial.emissiveFactor = material.emissiveFactor;
            gpuMaterial.metallicRoughnessOcclusion = glm::vec4(material.metallicFactor, material.roughnessFactor, material.occlusionStrength, 0.0f);
            gpuMaterial.textureIndices = glm::ivec4(static_cast<int32_t>(i));

            const auto debugPrefix = fmt::format("{}:{}", model.debugName, material.name);
            model.baseColorTextures[i] = metal_model_load_texture_slot(ctx, model, material.textures.baseColor, model.fallbackBaseColorTexture, debugPrefix);
            model.normalTextures[i] = metal_model_load_texture_slot(ctx, model, material.textures.normal, model.fallbackNormalTexture, debugPrefix);
            model.metallicRoughnessTextures[i] = metal_model_load_texture_slot(ctx, model, material.textures.metallicRoughness, model.fallbackMetallicRoughnessTexture, debugPrefix);
            model.emissiveTextures[i] = metal_model_load_texture_slot(ctx, model, material.textures.emissive, model.fallbackEmissiveTexture, debugPrefix);
            model.occlusionTextures[i] = metal_model_load_texture_slot(ctx, model, material.textures.occlusion, model.fallbackOcclusionTexture, debugPrefix);
        }
        model.gpuMaterials[i] = gpuMaterial;
    }

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_model_error(scene, "Could not create model material buffer: Metal device is not available.");
        return false;
    }

    auto materialsBuffer = [device newBufferWithBytes:model.gpuMaterials.data() length:model.gpuMaterials.size() * sizeof(metal::MetalGpuMaterial) options:MTLResourceStorageModeShared];
    if (!materialsBuffer)
    {
        report_model_error(scene, fmt::format("Could not create Metal material buffer for model: {}", model.createInfo.filename));
        return false;
    }

    materialsBuffer.label = ns_string(fmt::format("{}:Materials", model.debugName));
    retain_obj(model.materialsBuffer, materialsBuffer);
    return true;
}

bool metal_model_stage(metal::MetalContext& ctx, metal::MetalScene& scene, metal::MetalModel& model)
{
    if (model.vertexData.empty() || model.indexData.empty())
    {
        report_model_error(scene, fmt::format("Could not stage model '{}': vertex or index data is empty.", model.createInfo.filename));
        return false;
    }

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_model_error(scene, "Could not stage model: Metal device is not available.");
        return false;
    }

    auto vertexBuffer = [device newBufferWithBytes:model.vertexData.data() length:model.vertexData.size() options:MTLResourceStorageModeShared];
    auto indexBuffer = [device newBufferWithBytes:model.indexData.data() length:model.indexData.size() * sizeof(uint32_t) options:MTLResourceStorageModeShared];
    if (!vertexBuffer || !indexBuffer)
    {
        report_model_error(scene, fmt::format("Could not create Metal buffers for model: {}", model.createInfo.filename));
        return false;
    }

    vertexBuffer.label = ns_string(fmt::format("{}:Vertices", model.debugName));
    indexBuffer.label = ns_string(fmt::format("{}:Indices", model.debugName));

    retain_obj(model.vertexBuffer, vertexBuffer);
    retain_obj(model.indexBuffer, indexBuffer);
    model.staged = true;
    if (!metal_model_prepare_materials(ctx, scene, model))
    {
        return false;
    }

    if (model.createInfo.buildAS && !metal_model_build_acceleration_structures(ctx, scene, model))
    {
        return false;
    }

    return true;
}

} // namespace

namespace metal
{

std::shared_ptr<MetalModel> metal_model_create(MetalContext& ctx, MetalScene& scene, Geometry& geometry)
{
    auto loadPath = resolve_model_path(scene, geometry);
    if (loadPath.empty())
    {
        return nullptr;
    }

    ModelCreateInfo createInfo{
        .filename = loadPath.string(),
        .scale = geometry.loadScale,
        .uvOrigin = geometry.uvOrigin,
        .buildAS = geometry.buildAS
    };

    auto spMetalModel = std::make_shared<MetalModel>();
    spMetalModel->pGeometry = &geometry;
    spMetalModel->debugName = fmt::format("Model:{}", loadPath.filename().string());
    spMetalModel->vertexStride = layout_size(createInfo.vertexLayout);

    model_load(*spMetalModel, createInfo);
    if (spMetalModel->vertexData.empty() || spMetalModel->indexData.empty())
    {
        auto text = fmt::format("Could not load model: {}", loadPath.string());
        if (!spMetalModel->errors.empty())
        {
            text += "\n" + spMetalModel->errors;
        }
        report_model_error(scene, text, loadPath);
        return nullptr;
    }

    if (!metal_model_stage(ctx, scene, *spMetalModel))
    {
        metal_model_destroy(ctx, *spMetalModel);
        return nullptr;
    }

    return spMetalModel;
}

void metal_model_destroy(MetalContext& ctx, MetalModel& model)
{
    (void)ctx;
    release_obj(model.vertexBuffer);
    release_obj(model.indexBuffer);
    release_obj(model.bottomLevelAccelerationStructure);
    release_obj(model.topLevelAccelerationStructure);
    release_obj(model.accelerationScratchBuffer);
    release_obj(model.accelerationInstanceBuffer);
    release_obj(model.materialsBuffer);
    for (auto& texture : model.materialTextures)
    {
        metal_model_release_texture(texture);
    }
    model.materialTextures.clear();
    metal_model_release_texture(model.fallbackBaseColorTexture);
    metal_model_release_texture(model.fallbackNormalTexture);
    metal_model_release_texture(model.fallbackMetallicRoughnessTexture);
    metal_model_release_texture(model.fallbackEmissiveTexture);
    metal_model_release_texture(model.fallbackOcclusionTexture);
    model.gpuMaterials.clear();
    model.baseColorTextures.clear();
    model.normalTextures.clear();
    model.metallicRoughnessTextures.clear();
    model.emissiveTextures.clear();
    model.occlusionTextures.clear();
    model.staged = false;
    model.accelerationStructuresBuilt = false;
    model.vertexStride = 0;
}

} // namespace metal
