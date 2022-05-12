#include "vklive/vulkan/vulkan_model.h"

namespace vulkan
{
void model_load(VulkanContext& ctx, VulkanModel& model, const std::string& filename, const VertexLayout& layout, const ModelCreateInfo& createInfo, const int flags)
{
    model_load(model, filename, layout, createInfo, flags);
}

void model_stage(VulkanContext& ctx, VulkanModel& model)
{
    if (!model.vertexData.empty() && !model.indices.buffer)
    {
        // Vertex buffer
        // Index buffer
        model.vertices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eVertexBuffer, model.vertexData);
        model.indices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eIndexBuffer, model.indexData);

        debug_set_buffer_name(ctx.device, (VkBuffer)model.vertices.buffer, "Model::Vertices");
        debug_set_buffer_name(ctx.device, (VkBuffer)model.indices.buffer, "Model::Indices");
        
        debug_set_devicememory_name(ctx.device, model.vertices.memory, "Model::VerticesMemory");
        debug_set_devicememory_name(ctx.device, model.indices.memory, "Model::IndicesMemory");
    }
}

void model_load(VulkanContext& ctx, VulkanModel& model, const std::string& filename, const VertexLayout& layout, const glm::vec3& scale, const int flags)
{
    model_load(ctx, model, filename, layout, ModelCreateInfo{ glm::vec3(0.0f), scale, glm::vec2(1.0f) }, flags);
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

void model_destroy(VulkanContext& ctx, VulkanModel& model)
{
    buffer_destroy(ctx, model.vertices);
    buffer_destroy(ctx, model.indices);
}
} // namespace vulkan
