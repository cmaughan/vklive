#include <fmt/format.h>

#include <zest/file/runtree.h>

#include "vklive/vulkan/vulkan_command.h"
#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{

// Ray tracing acceleration structure
// Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
void createBottomLevelAccelerationStructure(VulkanContext& ctx, VulkanModel& model)
{
    // Setup identity transform matrix
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    // Is this the right way?  Can I pack these more efficiently into a single set of buffers and refer to them?
    for (const auto& part : model.parts)
    {
        std::vector<uint32_t> indices;
        std::vector<glm::vec3> vertices;

        for (uint32_t i = 0; i < part.indexCount; i++)
        {
            indices.push_back(model.indexData[part.indexBase + i] - part.vertexBase);
        }

        uint32_t vertexStride = layout_size(model.createInfo.vertexLayout);

        for (uint32_t i = 0; i < part.vertexCount; i++)
        {
            vertices.push_back(*(glm::vec3*)&model.vertexData[(part.vertexBase + i) * vertexStride]);
        }

        vertexStride = sizeof(glm::vec3);

        // Create buffers
        // For the sake of simplicity we won't stage the vertex data to the GPU memory
        auto vertexBuffer = buffer_create(ctx, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, vertices);

        auto indexBuffer = buffer_create(ctx, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, indices);

        auto transformBuffer = buffer_create(ctx, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, sizeof(VkTransformMatrixKHR), &transformMatrix);

        debug_set_buffer_name(ctx.device, vertexBuffer.buffer, "AS Vertices");
        debug_set_buffer_name(ctx.device, indexBuffer.buffer, "AS Indices");
        debug_set_buffer_name(ctx.device, transformBuffer.buffer, "AS Transform");

        vk::AccelerationStructureGeometryTrianglesDataKHR triangleData(
            vk::Format::eR32G32B32Sfloat,
            vertexBuffer.deviceAddress,
            vertexStride,
            vertices.size(),
            vk::IndexType::eUint32,
            indexBuffer.deviceAddress,
            transformBuffer.deviceAddress);

        vk::AccelerationStructureGeometryKHR accelerationStructureGeometry(vk::GeometryTypeKHR::eTriangles, triangleData, vk::GeometryFlagBitsKHR::eOpaque);

        vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo(
            vk::AccelerationStructureTypeKHR::eBottomLevel,
            vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);

        accelerationStructureBuildGeometryInfo.setGeometries(accelerationStructureGeometry);

        const auto numTriangles = indices.size() / 3;
        auto buildSizes = ctx.device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, numTriangles);

        AccelerationStructure bottomLevelAS;
        bottomLevelAS.buffer = buffer_create(ctx, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal, buildSizes.accelerationStructureSize);

        debug_set_buffer_name(ctx.device, bottomLevelAS.buffer.buffer, part.name + "_Buffer_BLAS");
        debug_set_devicememory_name(ctx.device, bottomLevelAS.buffer.memory, part.name + "_Memory_BLAS");

        vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo(vk::AccelerationStructureCreateFlagsKHR(), bottomLevelAS.buffer.buffer, 0, buildSizes.accelerationStructureSize, vk::AccelerationStructureTypeKHR::eBottomLevel);

        bottomLevelAS.handle = ctx.device.createAccelerationStructureKHR(accelerationStructureCreateInfo);
        debug_set_accelerationstructure_name(ctx.device, bottomLevelAS.handle, part.name + "_BLAS");

        // Create a small scratch buffer used during build of the bottom level acceleration structure
        auto scratchBuffer = buffer_create(ctx, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, buildSizes.buildScratchSize);

        vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo(
            vk::AccelerationStructureTypeKHR::eBottomLevel,
            vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
        accelerationBuildGeometryInfo.setDstAccelerationStructure(bottomLevelAS.handle);
        accelerationBuildGeometryInfo.setGeometries(accelerationStructureGeometry);
        accelerationBuildGeometryInfo.scratchData.deviceAddress = vk::DeviceAddress((void*)scratchBuffer.deviceAddress.deviceAddress);

        vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo(numTriangles);

        std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

        // Build the acceleration structure on the device via a one-time command buffer submission
        // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
        utils_with_command_buffer(ctx, [&](const vk::CommandBuffer& commandBuffer) {
            commandBuffer.buildAccelerationStructuresKHR(accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos);
        });

        bottomLevelAS.asDeviceAddress = ctx.device.getAccelerationStructureAddressKHR(bottomLevelAS.handle);

        bottomLevelAS.vertexOffset = part.vertexBase;

        model.accelerationStructures.push_back(bottomLevelAS);

        vulkan_buffer_destroy(ctx, scratchBuffer);
        vulkan_buffer_destroy(ctx, vertexBuffer);
        vulkan_buffer_destroy(ctx, transformBuffer);
        vulkan_buffer_destroy(ctx, indexBuffer);
    }
}

void createTopLevelAccelerationStructure(VulkanContext& ctx, VulkanModel& model)
{
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    for (auto& as : model.accelerationStructures)
    {
        instances.push_back(vk::AccelerationStructureInstanceKHR(transformMatrix, as.vertexOffset, 0xFF, 0, vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable, as.asDeviceAddress));
    }

    // Buffer for instance data
    auto instancesBuffer = buffer_create(
        ctx,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        instances);

    vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances; 
    accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque; 
    accelerationStructureGeometry.geometry.instances.sType = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;
    accelerationStructureGeometry.geometry.instances.arrayOfPointers = false;
    accelerationStructureGeometry.geometry.instances.data = instancesBuffer.deviceAddress;

    vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo(
        vk::AccelerationStructureTypeKHR::eTopLevel,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    accelerationStructureBuildGeometryInfo.setGeometries(accelerationStructureGeometry);

    auto buildSizes = ctx.device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, model.accelerationStructures.size());

    AccelerationStructure topLevelAS;
    topLevelAS.buffer = buffer_create(ctx, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal, buildSizes.accelerationStructureSize);

    debug_set_buffer_name(ctx.device, topLevelAS.buffer.buffer, model.debugName + "_Buffer_TLAS");
    debug_set_devicememory_name(ctx.device, topLevelAS.buffer.memory, model.debugName + "_Memory_TLAS");

    vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo(vk::AccelerationStructureCreateFlagsKHR(), topLevelAS.buffer.buffer, 0, buildSizes.accelerationStructureSize, vk::AccelerationStructureTypeKHR::eTopLevel);

    topLevelAS.handle = ctx.device.createAccelerationStructureKHR(accelerationStructureCreateInfo);
    debug_set_accelerationstructure_name(ctx.device, topLevelAS.handle, model.debugName + "_TLAS");

    // Create a small scratch buffer used during build of the bottom level acceleration structure
    auto scratchBuffer = buffer_create(ctx, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, buildSizes.buildScratchSize);

    vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo(
        vk::AccelerationStructureTypeKHR::eTopLevel,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    accelerationBuildGeometryInfo.setDstAccelerationStructure(topLevelAS.handle);
    accelerationBuildGeometryInfo.setGeometries(accelerationStructureGeometry);
    accelerationBuildGeometryInfo.scratchData.deviceAddress = vk::DeviceAddress((void*)scratchBuffer.deviceAddress.deviceAddress);

    vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo(model.accelerationStructures.size());

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

    // Build the acceleration structure on the device via a one-time command buffer submission
    // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
    utils_with_command_buffer(ctx, [&](const vk::CommandBuffer& commandBuffer) {
        commandBuffer.buildAccelerationStructuresKHR(accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos);
    });

    topLevelAS.asDeviceAddress = ctx.device.getAccelerationStructureAddressKHR(topLevelAS.handle);

    model.topLevelAS = topLevelAS;

    model.topLevelASDescriptor = vk::WriteDescriptorSetAccelerationStructureKHR(1, &model.topLevelAS.handle);

    vulkan_buffer_destroy(ctx, scratchBuffer);
}

// The top level acceleration structure contains the scene's object instances
void vulkan_model_build_acceleration_structure(VulkanContext& ctx, VulkanModel& model)
{
    if (!model.initAccel)
    {
        createBottomLevelAccelerationStructure(ctx, model);
        if (!model.accelerationStructures.empty())
        {
            createTopLevelAccelerationStructure(ctx, model);
        }
        model.initAccel = true;
    }
}

} // namespace vulkan


