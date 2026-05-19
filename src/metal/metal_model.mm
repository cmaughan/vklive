#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <vklive/metal/metal_model.h>

#include <fmt/format.h>

#include <zest/file/runtree.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_scene.h>
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
    model.staged = false;
    model.vertexStride = 0;
}

} // namespace metal
