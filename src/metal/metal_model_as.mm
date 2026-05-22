#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <vklive/metal/metal_model_as.h>

#include <algorithm>
#include <cstring>

#include <fmt/format.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_model.h>
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

std::string ns_string(NSString* string)
{
    return string ? std::string([string UTF8String]) : std::string();
}

void report_as_message(metal::MetalScene& scene, MessageSeverity severity, const std::string& text, const fs::path& path = fs::path())
{
    if (scene.pScene)
    {
        if (severity >= MessageSeverity::Error)
        {
            scene_report_error(*scene.pScene, severity, text, path);
            return;
        }

        Message msg;
        msg.text = text;
        msg.path = path.empty() ? scene.pScene->sceneGraphPath : path;
        msg.severity = severity;
        scene.pScene->warnings.push_back(msg);
    }
}

void report_as_error(metal::MetalScene& scene, const std::string& text, const fs::path& path = fs::path())
{
    report_as_message(scene, MessageSeverity::Error, text, path);
}

void report_as_warning(metal::MetalScene& scene, const std::string& text, const fs::path& path = fs::path())
{
    report_as_message(scene, MessageSeverity::Warning, text, path);
}

bool encode_build(id<MTLCommandQueue> commandQueue,
    id<MTLAccelerationStructure> accelerationStructure,
    MTLAccelerationStructureDescriptor* descriptor,
    id<MTLBuffer> scratchBuffer,
    metal::MetalScene& scene,
    const std::string& debugName)
{
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    if (!commandBuffer)
    {
        report_as_error(scene, fmt::format("Could not build Metal acceleration structure '{}': command buffer allocation failed.", debugName));
        return false;
    }

    commandBuffer.label = ns_string(fmt::format("{}:ASBuild", debugName));
    id<MTLAccelerationStructureCommandEncoder> encoder = [commandBuffer accelerationStructureCommandEncoder];
    if (!encoder)
    {
        report_as_error(scene, fmt::format("Could not build Metal acceleration structure '{}': command encoder allocation failed.", debugName));
        return false;
    }

    [encoder buildAccelerationStructure:accelerationStructure descriptor:descriptor scratchBuffer:scratchBuffer scratchBufferOffset:0];
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    if (commandBuffer.status == MTLCommandBufferStatusError)
    {
        report_as_error(scene, fmt::format("Could not build Metal acceleration structure '{}': {}", debugName, ns_string([commandBuffer.error localizedDescription])));
        return false;
    }

    return true;
}

id<MTLDevice> acceleration_structure_device(metal::MetalContext& ctx, metal::MetalScene& scene, metal::MetalModel& model, bool& supported)
{
    supported = false;
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_as_error(scene, "Could not build Metal acceleration structures: Metal device is not available.", model.createInfo.filename);
        return nil;
    }

    if (@available(macOS 11.0, *))
    {
        if (![device supportsRaytracing])
        {
            report_as_warning(scene,
                fmt::format("Metal acceleration structures requested for model '{}', but this macOS/Metal device does not support ray tracing acceleration structures; raster rendering will continue without them.",
                    model.createInfo.filename),
                model.createInfo.filename);
            return device;
        }
        supported = true;
        return device;
    }
    else
    {
        report_as_warning(scene,
            fmt::format("Metal acceleration structures requested for model '{}', but macOS 11.0 or newer is required; raster rendering will continue without them.", model.createInfo.filename),
            model.createInfo.filename);
        return device;
    }
}

} // namespace

namespace metal
{

bool metal_model_build_acceleration_structures(MetalContext& ctx, MetalScene& scene, MetalModel& model)
{
    if (model.accelerationStructuresBuilt)
    {
        return true;
    }

    if (!model.vertexBuffer || !model.indexBuffer || model.indexData.empty() || model.vertexStride == 0)
    {
        report_as_error(scene, fmt::format("Could not build Metal acceleration structures for model '{}': staged vertex or index buffers are missing.", model.createInfo.filename), model.createInfo.filename);
        return false;
    }

    bool supported = false;
    id<MTLDevice> device = acceleration_structure_device(ctx, scene, model, supported);
    if (!device)
    {
        return false;
    }
    if (!supported)
    {
        return true;
    }

    if (@available(macOS 11.0, *))
    {
        auto commandQueue = bridge<id<MTLCommandQueue>>(ctx.commandQueue);
        if (!commandQueue)
        {
            report_as_error(scene, "Could not build Metal acceleration structures: Metal command queue is not available.", model.createInfo.filename);
            return false;
        }

        release_obj(model.bottomLevelAccelerationStructure);
        release_obj(model.topLevelAccelerationStructure);
        release_obj(model.accelerationScratchBuffer);
        release_obj(model.accelerationInstanceBuffer);

        auto vertexBuffer = bridge<id<MTLBuffer>>(model.vertexBuffer);
        auto indexBuffer = bridge<id<MTLBuffer>>(model.indexBuffer);

        MTLAccelerationStructureTriangleGeometryDescriptor* triangleGeometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
        triangleGeometry.vertexBuffer = vertexBuffer;
        triangleGeometry.vertexBufferOffset = 0;
        triangleGeometry.vertexStride = model.vertexStride;
        if (@available(macOS 13.0, *))
        {
            triangleGeometry.vertexFormat = MTLAttributeFormatFloat3;
        }
        triangleGeometry.indexBuffer = indexBuffer;
        triangleGeometry.indexBufferOffset = 0;
        triangleGeometry.indexType = MTLIndexTypeUInt32;
        triangleGeometry.triangleCount = model.indexData.size() / 3;
        triangleGeometry.opaque = YES;

        MTLPrimitiveAccelerationStructureDescriptor* blasDescriptor = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
        blasDescriptor.geometryDescriptors = @[ triangleGeometry ];
        blasDescriptor.usage = MTLAccelerationStructureUsageNone;

        auto blasSizes = [device accelerationStructureSizesWithDescriptor:blasDescriptor];
        id<MTLAccelerationStructure> blas = [device newAccelerationStructureWithSize:blasSizes.accelerationStructureSize];
        if (!blas)
        {
            report_as_error(scene, fmt::format("Could not allocate Metal bottom-level acceleration structure for model '{}'.", model.createInfo.filename), model.createInfo.filename);
            return false;
        }
        blas.label = ns_string(fmt::format("{}:BLAS", model.debugName));

        MTLAccelerationStructureInstanceDescriptor instance;
        std::memset(&instance, 0, sizeof(instance));
        instance.transformationMatrix = MTLPackedFloat4x3(
            MTLPackedFloat3(1.0f, 0.0f, 0.0f),
            MTLPackedFloat3(0.0f, 1.0f, 0.0f),
            MTLPackedFloat3(0.0f, 0.0f, 1.0f),
            MTLPackedFloat3(0.0f, 0.0f, 0.0f));
        instance.mask = 0xFF;
        instance.accelerationStructureIndex = 0;

        id<MTLBuffer> instanceBuffer = [device newBufferWithBytes:&instance length:sizeof(instance) options:MTLResourceStorageModeShared];
        if (!instanceBuffer)
        {
            report_as_error(scene, fmt::format("Could not allocate Metal acceleration-structure instance buffer for model '{}'.", model.createInfo.filename), model.createInfo.filename);
            return false;
        }
        instanceBuffer.label = ns_string(fmt::format("{}:ASInstances", model.debugName));

        MTLInstanceAccelerationStructureDescriptor* tlasDescriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
        tlasDescriptor.instanceDescriptorBuffer = instanceBuffer;
        tlasDescriptor.instanceDescriptorBufferOffset = 0;
        tlasDescriptor.instanceDescriptorStride = sizeof(MTLAccelerationStructureInstanceDescriptor);
        tlasDescriptor.instanceCount = 1;
        tlasDescriptor.instancedAccelerationStructures = @[ blas ];
        if (@available(macOS 12.0, *))
        {
            tlasDescriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeDefault;
        }
        tlasDescriptor.usage = MTLAccelerationStructureUsageNone;

        auto tlasSizes = [device accelerationStructureSizesWithDescriptor:tlasDescriptor];
        id<MTLAccelerationStructure> tlas = [device newAccelerationStructureWithSize:tlasSizes.accelerationStructureSize];
        if (!tlas)
        {
            report_as_error(scene, fmt::format("Could not allocate Metal top-level acceleration structure for model '{}'.", model.createInfo.filename), model.createInfo.filename);
            return false;
        }
        tlas.label = ns_string(fmt::format("{}:TLAS", model.debugName));

        const auto scratchSize = std::max(blasSizes.buildScratchBufferSize, tlasSizes.buildScratchBufferSize);
        id<MTLBuffer> scratchBuffer = [device newBufferWithLength:scratchSize options:MTLResourceStorageModePrivate];
        if (!scratchBuffer)
        {
            report_as_error(scene, fmt::format("Could not allocate Metal acceleration-structure scratch buffer for model '{}'.", model.createInfo.filename), model.createInfo.filename);
            return false;
        }
        scratchBuffer.label = ns_string(fmt::format("{}:ASScratch", model.debugName));

        if (!encode_build(commandQueue, blas, blasDescriptor, scratchBuffer, scene, fmt::format("{}:BLAS", model.debugName)) ||
            !encode_build(commandQueue, tlas, tlasDescriptor, scratchBuffer, scene, fmt::format("{}:TLAS", model.debugName)))
        {
            return false;
        }

        retain_obj(model.bottomLevelAccelerationStructure, blas);
        retain_obj(model.topLevelAccelerationStructure, tlas);
        retain_obj(model.accelerationScratchBuffer, scratchBuffer);
        retain_obj(model.accelerationInstanceBuffer, instanceBuffer);
        model.accelerationStructuresBuilt = true;
        return true;
    }

    return true;
}

} // namespace metal
