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

        uint32_t vertexStride = layout_size(model.layout);

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

    uint32_t index = 0;
    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    for (auto& as : model.accelerationStructures)
    {
        instances.push_back(vk::AccelerationStructureInstanceKHR(transformMatrix, index++, 0xFF, 0, vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable, as.asDeviceAddress));
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

    vulkan_buffer_destroy(ctx, scratchBuffer);
}

#if 0

/*
    Gets the device address from a buffer that's required for some of the buffers used for ray tracing
*/

/*
    Set up a storage image that the ray generation shader will be writing to
*/
void createStorageImage()
{
    VkImageCreateInfo image = vks::initializers::imageCreateInfo();
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = swapChain.colorFormat;
    image.extent.width = width;
    image.extent.height = height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &storageImage.image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, storageImage.image, &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo = vks::initializers::memoryAllocateInfo();
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &storageImage.memory));
    VK_CHECK_RESULT(vkBindImageMemory(device, storageImage.image, storageImage.memory, 0));

    VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.format = swapChain.colorFormat;
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.image = storageImage.image;
    VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &storageImage.view));

    VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vks::tools::setImageLayout(cmdBuffer, storageImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    vulkanDevice->flushCommandBuffer(cmdBuffer, queue);
}
#endif


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

    // createTopLevelAccelerationStructure();
    //  createStorageImage();
}

} // namespace vulkan
