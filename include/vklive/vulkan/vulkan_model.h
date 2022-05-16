#pragma once

#include "vklive/model.h"

#include "vulkan_context.h"
#include "vulkan_buffer.h"

namespace vulkan
{
struct VulkanModel : Model
{
    VulkanBuffer vertices;
    VulkanBuffer indices;
};

void model_load(VulkanContext& ctx, VulkanModel& model, const std::string& filename, const VertexLayout& layout, const ModelCreateInfo& createInfo, const int flags);
void model_load(VulkanContext& ctx, VulkanModel& model, const std::string& filename, const VertexLayout& layout, const glm::vec3& scale = glm::vec3(1.0f), const int flags = DefaultModelFlags);
vk::Format component_format(Component component);
void model_destroy(VulkanContext& ctx, VulkanModel& model);
void model_stage(VulkanContext& ctx, VulkanModel& model);
} // namespace vulkan
