#pragma once

#include <vklive/vulkan/vulkan_context.h>
#include <zest/ui/nanovg.h>

struct Scene;

namespace vulkan
{

void vulkan_nanovg_init(VulkanContext& ctx);
void vulkan_nanovg_draw(VulkanContext& ctx, VkRenderPass renderPass, VkCommandBuffer cmd);

} // namespace vulkan
