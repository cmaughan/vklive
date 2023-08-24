#pragma once

#include "vulkan_context.h"
namespace vulkan
{

struct VulkanContext;

struct BufferAllocation
{
    vk::Device device;
    vk::DeviceMemory memory;
    vk::DeviceSize size{ 0 };
    vk::DeviceSize alignment{ 0 };
    vk::DeviceSize allocSize{ 0 };
    void* mapped{ nullptr };
    vk::MemoryPropertyFlags memoryPropertyFlags; // @brief Memory propertys flags to be filled by external source at buffer creation (to query at some later point)
};

struct VulkanBuffer : public BufferAllocation
{
    vk::Buffer buffer;
    vk::BufferUsageFlags usageFlags; // @brief Usage flags to be filled by external source at buffer creation (to query at some later point)
    vk::DescriptorBufferInfo descriptor;
    vk::DeviceOrHostAddressConstKHR deviceAddress;
};

void vulkan_buffer_destroy(VulkanContext& ctx, VulkanBuffer& buffer);

void* buffer_map(VulkanContext& ctx, VulkanBuffer& buffer, size_t offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

template <typename T = void>
T* buffer_map(VulkanContext& ctx, VulkanBuffer& buffer, size_t offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
{
    return (T*)buffer_map(ctx, buffer, offset, size);
}

void buffer_unmap(VulkanContext& ctx, VulkanBuffer& buffer);

VulkanBuffer buffer_create_on_device(VulkanContext& ctx, const vk::BufferUsageFlags& usageFlags, vk::DeviceSize size);
VulkanBuffer buffer_create(VulkanContext& ctx, const vk::BufferUsageFlags& usageFlags, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size, void* pData = nullptr);

VulkanBuffer buffer_create_staging(VulkanContext& ctx, vk::DeviceSize size, const void* data = nullptr);
template <typename T>
VulkanBuffer buffer_create_in_memory(VulkanContext& ctx, const std::vector<T>& data)
{
    return buffer_create_staging(ctx, data.size() * sizeof(T), (void*)data.data());
}

template <typename T>
VulkanBuffer buffer_create_in_memory(VulkanContext& ctx, const T& data)
{
    return buffer_create_staging(ctx, sizeof(T), &data);
}

VulkanBuffer buffer_stage_to_device(VulkanContext& ctx, const vk::BufferUsageFlags& usage, size_t size, const void* data);
template <typename T>
VulkanBuffer buffer_stage_to_device(VulkanContext& ctx, const vk::BufferUsageFlags& usage, const std::vector<T>& data)
{
    return buffer_stage_to_device(ctx, usage, sizeof(T) * data.size(), data.data());
}
template <typename T>
VulkanBuffer buffer_stage_to_device(VulkanContext& ctx, const vk::BufferUsageFlags& usage, const T& data)
{
    return buffer_stage_to_device(ctx, usage, sizeof(T), (void*)&data);
}

void buffer_create_or_resize(VulkanContext& ctx, vk::Buffer& buffer, vk::DeviceMemory& buffer_memory, vk::DeviceSize& p_buffer_size, size_t new_size, vk::BufferUsageFlagBits usage);

template<class T>
VulkanBuffer buffer_create(VulkanContext& ctx, const vk::BufferUsageFlags& usageFlags, const vk::MemoryPropertyFlags& memoryPropertyFlags, const std::vector<T>& data)
{
    return buffer_create(ctx, usageFlags, memoryPropertyFlags, data.size() * sizeof(T), (void*)data.data());
}

} // namespace vulkan
