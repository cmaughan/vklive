#pragma once

#include "vulkan_context.h"

namespace vulkan
{
struct VulkanPassTargets;

void vulkan_framebuffer_create(VulkanContext& ctx, vk::Framebuffer& framebuffer, const VulkanPassTargets& passTargets, const vk::RenderPass& renderPass);
void framebuffer_destroy(VulkanContext& ctx, vk::Framebuffer& frame);

} // namespace vulkan
