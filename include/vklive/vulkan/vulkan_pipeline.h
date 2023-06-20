#include <zest/file/file.h>

#include "vulkan_context.h"
#include "vulkan_render.h"
#include "vulkan_utils.h"
#include "vulkan_shader.h"

namespace vulkan
{
vk::Pipeline pipeline_create(VulkanContext& ctx, const VertexLayout& vertexLayout, const vk::PipelineLayout& layout, VulkanPassTargets& passTargets, const std::vector<vk::PipelineShaderStageCreateInfo>& shaders);
}
