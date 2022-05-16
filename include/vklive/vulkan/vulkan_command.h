#pragma once

#include "vulkan_context.h"

namespace vulkan
{

void command_submit_wait(VulkanContext& ctx, vk::Queue const& queue, vk::CommandBuffer const& commandBuffer);
vk::CommandPool utils_get_command_pool(VulkanContext& ctx);
vk::CommandBuffer utils_create_command_buffer(VulkanContext& ctx, vk::CommandBufferLevel level);
void utils_with_command_buffer(VulkanContext& ctx, const std::function<void(const vk::CommandBuffer& commandBuffer)>& f);
void utils_flush_command_buffer(VulkanContext& ctx, vk::CommandBuffer& commandBuffer);

}
