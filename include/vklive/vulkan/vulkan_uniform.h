#pragma once

#include "vulkan_buffer.h"
#include "vulkan_utils.h"

namespace vulkan
{
VulkanBuffer uniform_create(VulkanContext& ctx, vk::DeviceSize size);

template <typename T>
VulkanBuffer uniform_create(VulkanContext& ctx, const T& data)
{
    auto result = uniform_create(ctx, vk::DeviceSize(sizeof(T)));
    utils_copy_to_memory(ctx, result.memory, data);
    return result;
}
} // namespace vulkan
