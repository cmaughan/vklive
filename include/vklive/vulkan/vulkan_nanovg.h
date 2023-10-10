#pragma once

#include <vklive/vulkan/vulkan_context.h>
#include <zest/ui/nanovg.h>

struct Scene;

namespace vulkan
{

void vulkan_nanovg_init(VulkanContext& ctx);

void vulkan_nanovg_begin(VulkanContext& ctx, VulkanPass& vulkanPass, vk::CommandBuffer& cmd);
void vulkan_nanovg_end(VulkanContext& ctx);

} // namespace vulkan
