#include <fmt/format.h>

#include <zest/file/runtree.h>

#include "vklive/vulkan/vulkan_buffer.h"
#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{

// Holds data for a ray tracing scratch buffer that is used as a temporary storage
struct RayTracingScratchBuffer
{
    uint64_t deviceAddress = 0;
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

// Ray tracing acceleration structure
struct AccelerationStructure
{
    VkAccelerationStructureKHR handle;
    uint64_t deviceAddress = 0;
    VkDeviceMemory memory;
    VkBuffer buffer;
};

// Vertex layout for this example
VertexLayout g_vertexLayout{ {
    Component::VERTEX_COMPONENT_POSITION,
    Component::VERTEX_COMPONENT_UV,
    Component::VERTEX_COMPONENT_COLOR,
    Component::VERTEX_COMPONENT_NORMAL,
} };

void vulkan_model_load(VulkanContext& ctx,
    VulkanModel& model,
    const std::string& filename,
    const VertexLayout& layout,
    const ModelCreateInfo& createInfo,
    const int flags)
{
    // Call the model class
    model_load(model, filename, layout, createInfo, flags);
}

void vulkan_model_load(VulkanContext& ctx,
    VulkanModel& model,
    const std::string& filename,
    const VertexLayout& layout,
    const glm::vec3& scale,
    const int flags)
{
    vulkan_model_load(ctx, model, filename, layout, ModelCreateInfo{ glm::vec3(0.0f), scale, glm::vec2(1.0f) }, flags);
}

void vulkan_model_stage(VulkanContext& ctx,
    VulkanModel& model)
{
    if (!model.vertexData.empty() && !model.indices.buffer)
    {
        // Vertex buffer
        // Index buffer
        model.vertices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eVertexBuffer, model.vertexData);
        model.indices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eIndexBuffer, model.indexData);

        debug_set_buffer_name(ctx.device, (VkBuffer)model.vertices.buffer, fmt::format("{}:Vertices", model.debugName));
        debug_set_buffer_name(ctx.device, (VkBuffer)model.indices.buffer, fmt::format("{}:Indices", model.debugName));

        debug_set_devicememory_name(ctx.device, model.vertices.memory, fmt::format("{}:VerticesMemory", model.debugName));
        debug_set_devicememory_name(ctx.device, model.indices.memory, fmt::format("{}:IndicesMemory", model.debugName));
    }
}

vk::Format component_format(Component component)
{
    switch (component)
    {
    case VERTEX_COMPONENT_UV:
        return vk::Format::eR32G32Sfloat;
    case VERTEX_COMPONENT_DUMMY_FLOAT:
        return vk::Format::eR32Sfloat;
    case VERTEX_COMPONENT_DUMMY_INT:
        return vk::Format::eR32Sint;
    case VERTEX_COMPONENT_DUMMY_VEC4:
        return vk::Format::eR32G32B32A32Sfloat;
    case VERTEX_COMPONENT_DUMMY_INT4:
        return vk::Format::eR32G32B32A32Sint;
    case VERTEX_COMPONENT_DUMMY_UINT4:
        return vk::Format::eR32G32B32A32Uint;
    default:
        return vk::Format::eR32G32B32Sfloat;
    }
}

void vulkan_model_destroy(VulkanContext& ctx, VulkanModel& model)
{
    vulkan_buffer_destroy(ctx, model.vertices);
    vulkan_buffer_destroy(ctx, model.indices);
}

uint64_t getBufferDeviceAddress(VulkanContext& ctx, VkBuffer buffer)
{
    VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
    bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAI.buffer = buffer;
    return ctx.vkGetBufferDeviceAddressKHR(ctx.device, &bufferDeviceAI);
}

void createAccelerationStructureBuffer(VulkanContext& ctx, AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
{
    /*
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    ctx.device.createBuffer(&bufferCreateInfo, nullptr, &accelerationStructure.buffer));

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(ctx.device, accelerationStructure.buffer, &memoryRequirements);

    VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
    memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = utils_memory_type(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ctx.device.vkAllocateMemory(&memoryAllocateInfo, nullptr, &accelerationStructure.memory));

    ctx.device.vkBindBufferMemory(accelerationStructure.buffer, accelerationStructure.memory, 0));
    */
}

void createBottomLevelAccelerationStructure(VulkanContext& ctx)
{
    /*
    AccelerationStructure bottomLevelAS{};
    AccelerationStructure topLevelAS{};

    vks::Buffer vertexBuffer;
    vks::Buffer indexBuffer;
    uint32_t indexCount;

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
    auto indexCount = static_cast<uint32_t>(indices.size());

    // Setup identity transform matrix
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    //    buffer_create(ctx, )

    // Create buffers
    // For the sake of simplicity we won't stage the vertex data to the GPU memory
    // Vertex buffer
    ctx.device.createBuffer(
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &vertexBuffer,
        vertices.size() * sizeof(Vertex),
        vertices.data()));

    // Index buffer
    ctx.device.createBuffer(
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &indexBuffer,
        indices.size() * sizeof(uint32_t),
        indices.data()));

    // Transform buffer
    ctx.device.createBuffer(
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
        ctx.device,
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
    vkCreateAccelerationStructureKHR(ctx.device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle);

    // Create a small scratch buffer used during build of the bottom level acceleration structure
    RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

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
    VkCommandBuffer commandBuffer = ctx.device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBuildAccelerationStructuresKHR(
        commandBuffer,
        1,
        &accelerationBuildGeometryInfo,
        accelerationBuildStructureRangeInfos.data());
    vulkanDevice->flushCommandBuffer(commandBuffer, ctx.queue);

    VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
    accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS.handle;
    bottomLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(ctx.device, &accelerationDeviceAddressInfo);

    deleteScratchBuffer(scratchBuffer);
    */
}

std::shared_ptr<VulkanModel> vulkan_model_create(VulkanContext& ctx,
    VulkanScene& vulkanScene,
    const Geometry& geom)
{
    auto spVulkanModel = std::make_shared<VulkanModel>(geom);

    // The geometry may be user geom or a model loaded from run tree; so just check it is available
    fs::path loadPath;
    if (geom.type == GeometryType::Model)
    {
        loadPath = geom.path;
    }
    else if (geom.type == GeometryType::Rect)
    {
        loadPath = Zest::runtree_find_path("models/quad.obj");
        if (loadPath.empty())
        {
            scene_report_error(*vulkanScene.pScene, MessageSeverity::Error,
                fmt::format("Could not load default asset: {}", "runtree/models/quad.obj"));
            return spVulkanModel;
        }
    }

    vulkan_model_load(ctx, *spVulkanModel, loadPath.string(), g_vertexLayout, geom.loadScale);

    // Success?
    if (spVulkanModel->vertexData.empty())
    {
        auto txt = fmt::format("Could not load model: {}", loadPath.string());
        if (!spVulkanModel->errors.empty())
        {
            txt += "\n" + spVulkanModel->errors;
        }
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, txt);
    }
    else
    {
        // Store at original path, even though we may have subtituted geometry for preset paths
        vulkanScene.models[geom.path] = spVulkanModel;
    }

    spVulkanModel->debugName = fmt::format("Model:{}", loadPath.filename().string());
    return spVulkanModel;
}

} // namespace vulkan
