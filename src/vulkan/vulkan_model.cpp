#include <fmt/format.h>

#include <zest/file/runtree.h>

#include "vklive/vulkan/vulkan_buffer.h"
#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{

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
        model.vertices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer, model.vertexData);
        model.indices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer, model.indexData);

        debug_set_buffer_name(ctx.device, (VkBuffer)model.vertices.buffer, fmt::format("{}:Vertices", model.debugName));
        debug_set_buffer_name(ctx.device, (VkBuffer)model.indices.buffer, fmt::format("{}:Indices", model.debugName));

        debug_set_devicememory_name(ctx.device, model.vertices.memory, fmt::format("{}:VerticesMemory", model.debugName));
        debug_set_devicememory_name(ctx.device, model.indices.memory, fmt::format("{}:IndicesMemory", model.debugName));

        model.verticesDescriptor = vk::DescriptorBufferInfo(model.vertices.buffer, 0, VK_WHOLE_SIZE);
        model.indicesDescriptor = vk::DescriptorBufferInfo(model.indices.buffer, 0, VK_WHOLE_SIZE);
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

    // AS
    for (auto& as : model.accelerationStructures)
    {
        vulkan_buffer_destroy(ctx, as.buffer);
        if (as.handle)
        {
            ctx.device.destroyAccelerationStructureKHR(as.handle);
        }
    }
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
