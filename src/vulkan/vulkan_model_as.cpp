#include <fmt/format.h>

#include <zest/file/runtree.h>

#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_utils.h"
#include "vklive/vulkan/vulkan_command.h"

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
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, vertices.size() * sizeof(glm::vec3));

        auto indexBuffer = buffer_create(ctx, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, indices.size() * sizeof(uint32_t));

        auto transformBuffer = buffer_create(ctx, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, sizeof(VkTransformMatrixKHR));

        debug_set_buffer_name(ctx.device, vertexBuffer.buffer, "AS Vertices");
        debug_set_buffer_name(ctx.device, indexBuffer.buffer, "AS Indices");
        debug_set_buffer_name(ctx.device, transformBuffer.buffer, "AS Transform");

        auto pVerts = buffer_map(ctx, vertexBuffer);
        memcpy(pVerts, &vertices[0], vertices.size() * sizeof(glm::vec3));
        buffer_unmap(ctx, vertexBuffer);

        auto pIndex = buffer_map(ctx, indexBuffer);
        memcpy(pIndex, &indices[0], indices.size() * sizeof(uint32_t));
        buffer_unmap(ctx, indexBuffer);

        auto pTransform = buffer_map(ctx, transformBuffer);
        memcpy(pTransform, &transformMatrix, sizeof(transformMatrix));
        buffer_unmap(ctx, transformBuffer);

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
#if 0

/*
    Gets the device address from a buffer that's required for some of the buffers used for ray tracing
*/

/*
    Set up a storage image that the ray generation shader will be writing to
*/
/*
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
*/


// The top level acceleration structure contains the scene's object instances
/*
void createTopLevelAccelerationStructure()
{
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform = transformMatrix;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = bottomLevelAS.deviceAddress;

    // Buffer for instance data
    vks::Buffer instancesBuffer;
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &instancesBuffer,
        sizeof(VkAccelerationStructureInstanceKHR),
        &instance));

    VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
    instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

    // Get size info
    // The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command,
    // except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    uint32_t primitive_count = 1;

    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelerationStructureBuildGeometryInfo,
        &primitive_count,
        &accelerationStructureBuildSizesInfo);

    createAccelerationStructureBuffer(topLevelAS, accelerationStructureBuildSizesInfo);

    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
    accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureCreateInfo.buffer = topLevelAS.buffer;
    accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
    accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &topLevelAS.handle);

    // Create a small scratch buffer used during build of the top level acceleration structure
    // RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

    VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
    accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS.handle;
    accelerationBuildGeometryInfo.geometryCount = 1;
    accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
    accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
    accelerationStructureBuildRangeInfo.primitiveCount = 1;
    accelerationStructureBuildRangeInfo.primitiveOffset = 0;
    accelerationStructureBuildRangeInfo.firstVertex = 0;
    accelerationStructureBuildRangeInfo.transformOffset = 0;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

    // Build the acceleration structure on the device via a one-time command buffer submission
    // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
    VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBuildAccelerationStructuresKHR(
        commandBuffer,
        1,
        &accelerationBuildGeometryInfo,
        accelerationBuildStructureRangeInfos.data());
    vulkanDevice->flushCommandBuffer(commandBuffer, queue);

    VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
    accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accelerationDeviceAddressInfo.accelerationStructure = topLevelAS.handle;
    topLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

    // deleteScratchBuffer(scratchBuffer);
    instancesBuffer.destroy();
}*/
#endif
void vulkan_model_build_acceleration_structure(VulkanContext& ctx, VulkanModel& model)
{
    if (!model.initAccel)
    {
        createBottomLevelAccelerationStructure(ctx, model);
    }
    model.initAccel = true;

    // createTopLevelAccelerationStructure();
    //  createStorageImage();
}

} // namespace vulkan
