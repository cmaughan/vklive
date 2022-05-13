#include "vulkan_context.h"
#include "vulkan_render.h"
#include "vulkan_utils.h"
#include "vulkan_shader.h"

#include <vklive/file/file.h>

namespace vulkan
{
vk::Pipeline pipeline_create(VulkanContext& ctx, const VertexLayout& vertexLayout, const vk::PipelineLayout& layout, const vk::RenderPass& renderPass, const std::vector<vk::PipelineShaderStageCreateInfo>& shaders);
}
