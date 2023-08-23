#include <fmt/format.h>

#include <zest/file/runtree.h>

#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{

// Holds data for a ray tracing scratch buffer that is used as a temporary storage
struct RayTracingScratchBuffer
{
    uint64_t deviceAddress = 0;
    vk::Buffer handle;
    vk::DeviceMemory memory;
};

// Ray tracing acceleration structure
struct AccelerationStructure
{
    vk::AccelerationStructureTypeKHR handle;
    uint64_t deviceAddress = 0;
    vk::DeviceMemory memory;
    vk::Buffer buffer;
};

// Create a scratch buffer to hold temporary data for a ray tracing acceleration structure
RayTracingScratchBuffer createScratchBuffer(VulkanContext& ctx, VkDeviceSize size)
{
    RayTracingScratchBuffer scratchBuffer{};

    // Create buffer
    scratchBuffer.handle = ctx.device.createBuffer(vk::BufferCreateInfo(vk::BufferCreateFlags(), size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress));

    auto req = ctx.device.getBufferMemoryRequirements(scratchBuffer.handle);

    // Allocate memory for buffer
    auto memAllocFlagsInfo = vk::MemoryAllocateFlagsInfo(vk::MemoryAllocateFlagBits::eDeviceAddress);
    auto allocInfo = vk::MemoryAllocateInfo(req.size, utils_memory_type(ctx, vk::MemoryPropertyFlagBits::eDeviceLocal, req.memoryTypeBits));
    allocInfo.pNext = &memAllocFlagsInfo;
    scratchBuffer.memory = ctx.device.allocateMemory(allocInfo);

    // Bind memory to buffer
    ctx.device.bindBufferMemory(scratchBuffer.handle, scratchBuffer.memory, 0);

    // get the gpu address of the memory
    scratchBuffer.deviceAddress = ctx.device.getBufferAddress(vk::BufferDeviceAddressInfo(scratchBuffer.handle));

    return scratchBuffer;
}

void deleteScratchBuffer(VulkanContext& ctx, RayTracingScratchBuffer& scratchBuffer)
{
    if (scratchBuffer.memory)
    {
        ctx.device.freeMemory(scratchBuffer.memory);
    }
    if (scratchBuffer.handle)
    {
        ctx.device.destroyBuffer(scratchBuffer.handle);
    }
}

#if 0
void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
{
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    // VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer));
    VkMemoryRequirements req{};
    // vkGetBufferMemoryRequirements(device, accelerationStructure.buffer, &req);
    VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
    memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
    memoryAllocateInfo.allocationSize = req.size;
    // memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    // VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory));
    // VK_CHECK_RESULT(vkBindBufferMemory(device, accelerationStructure.buffer, accelerationStructure.memory, 0));
}

/*
    Gets the device address from a buffer that's required for some of the buffers used for ray tracing
*/
uint64_t getBufferDeviceAddress(VkBuffer buffer)
{
    VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
    bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAI.buffer = buffer;
    // return vkGetBufferDeviceAddressKHR(device, &bufferDeviceAI);
    return 0;
}

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

/*
    Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
*/
void createBottomLevelAccelerationStructure()
{
    // Setup vertices for a single triangle
    struct Vertex
    {
        float pos[3];
    };
    std::vector<Vertex> vertices = {
        { { 1.0f, 1.0f, 0.0f } },
        { { -1.0f, 1.0f, 0.0f } },
        { { 0.0f, -1.0f, 0.0f } }
    };

    // Setup indices
    std::vector<uint32_t> indices = { 0, 1, 2 };
    indexCount = static_cast<uint32_t>(indices.size());

    // Setup identity transform matrix
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    // Create buffers
    // For the sake of simplicity we won't stage the vertex data to the GPU memory
    // Vertex buffer
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &vertexBuffer,
        vertices.size() * sizeof(Vertex),
        vertices.data()));
    // Index buffer
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &indexBuffer,
        indices.size() * sizeof(uint32_t),
        indices.data()));
    // Transform buffer
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &transformBuffer,
        sizeof(VkTransformMatrixKHR),
        &transformMatrix));

    VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
    VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
    VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

    vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(vertexBuffer.buffer);
    indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(indexBuffer.buffer);
    transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer.buffer);

    // Build
    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
    accelerationStructureGeometry.geometry.triangles.maxVertex = 3;
    accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
    accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;
    accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = 0;
    accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;
    accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;

    // Get size info
    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    const uint32_t numTriangles = 1;
    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelerationStructureBuildGeometryInfo,
        &numTriangles,
        &accelerationStructureBuildSizesInfo);

    createAccelerationStructureBuffer(bottomLevelAS, accelerationStructureBuildSizesInfo);

    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
    accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureCreateInfo.buffer = bottomLevelAS.buffer;
    accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
    accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle);

    // Create a small scratch buffer used during build of the bottom level acceleration structure
    // RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

    VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
    accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    accelerationBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS.handle;
    accelerationBuildGeometryInfo.geometryCount = 1;
    accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
    accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
    accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
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
    accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS.handle;
    bottomLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

    // deleteScratchBuffer(scratchBuffer);
}

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
    auto scratch = createScratchBuffer(ctx, 1000);
    deleteScratchBuffer(ctx, scratch);
    // createBottomLevelAccelerationStructure();
    // createTopLevelAccelerationStructure();
    //  createStorageImage();
}

} // namespace vulkan
