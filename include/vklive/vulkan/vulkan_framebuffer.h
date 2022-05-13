#pragma once

#include <glm/glm.hpp>
#include "vulkan_context.h"

namespace vulkan
{
struct VulkanFrameBuffer
{
    vk::Framebuffer framebuffer;
};

void framebuffer_create(VulkanContext& ctx, VulkanFrameBuffer& frame, const std::vector<VulkanImage>& colorBuffers, VulkanImage* pDepth, const vk::RenderPass& renderPass);
void framebuffer_destroy(VulkanContext& ctx, VulkanFrameBuffer& frame);

} // namespace vulkan
