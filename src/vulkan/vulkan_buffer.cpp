#include "vklive/vulkan/vulkan_buffer.h"
#include "vklive/vulkan/vulkan_command.h"
#include "vklive/vulkan/vulkan_debug.h"
#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{
void vulkan_buffer_destroy(VulkanContext& ctx, VulkanBuffer& buffer)
{
    if (!buffer.buffer)
    {
        return;
    }

    if (nullptr != buffer.mapped)
    {
        buffer_unmap(ctx, buffer);
    }

    if (buffer.memory)
    {
        ctx.device.freeMemory(buffer.memory);
        buffer.memory = vk::DeviceMemory();
    }

    ctx.device.destroy(buffer.buffer);
    buffer.buffer = vk::Buffer{};
}

void buffer_unmap(VulkanContext& ctx, VulkanBuffer& buffer)
{
    ctx.device.unmapMemory(buffer.memory);
    buffer.mapped = nullptr;
}

void buffer_create_or_resize(VulkanContext& ctx, vk::Buffer& buffer, vk::DeviceMemory& buffer_memory, vk::DeviceSize& p_buffer_size, size_t new_size, vk::BufferUsageFlagBits usage)
{
    VkResult err;
    if (buffer)
    {
        ctx.device.destroyBuffer(buffer);
    }

    if (buffer_memory)
    {
        ctx.device.freeMemory(buffer_memory);
    }

    vk::DeviceSize vertex_buffer_size_aligned = ((new_size - 1) / ctx.BufferMemoryAlignment + 1) * ctx.BufferMemoryAlignment;
    buffer = ctx.device.createBuffer(vk::BufferCreateInfo({}, vertex_buffer_size_aligned, usage, vk::SharingMode::eExclusive));

    auto req = ctx.device.getBufferMemoryRequirements(buffer);
    ctx.BufferMemoryAlignment = (ctx.BufferMemoryAlignment > req.alignment) ? ctx.BufferMemoryAlignment : req.alignment;
    buffer_memory = ctx.device.allocateMemory(vk::MemoryAllocateInfo(req.size, utils_memory_type(ctx, vk::MemoryPropertyFlagBits::eHostVisible, req.memoryTypeBits)));

    ctx.device.bindBufferMemory(buffer, buffer_memory, 0);
    p_buffer_size = req.size;
}

VulkanBuffer buffer_stage_to_device(VulkanContext& ctx, const vk::BufferUsageFlags& usage, size_t size, const void* data)
{
    VulkanBuffer staging = buffer_create_staging(ctx, size, data);
    VulkanBuffer result = buffer_create_on_device(ctx, usage | vk::BufferUsageFlagBits::eTransferDst, size);
    utils_with_command_buffer(ctx, [&](vk::CommandBuffer copyCmd) {
        debug_set_commandbuffer_name(ctx.device, copyCmd, "Buffer::StageToDevice");
        copyCmd.copyBuffer(staging.buffer, result.buffer, vk::BufferCopy(0, 0, size));
    });
    vulkan_buffer_destroy(ctx, staging);
    return result;
}

VulkanBuffer buffer_create(VulkanContext& ctx, const vk::BufferUsageFlags& usageFlags, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size)
{
    VulkanBuffer result;
    result.size = size;
    result.descriptor.range = VK_WHOLE_SIZE;
    result.descriptor.offset = 0;

    vk::BufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.usage = usageFlags;
    bufferCreateInfo.size = size;

    result.descriptor.buffer = result.buffer = ctx.device.createBuffer(bufferCreateInfo);

    vk::MemoryRequirements memReqs = ctx.device.getBufferMemoryRequirements(result.buffer);
    vk::MemoryAllocateInfo memAlloc;
    result.allocSize = memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = utils_memory_type(ctx, memoryPropertyFlags, memReqs.memoryTypeBits);
    result.memory = ctx.device.allocateMemory(memAlloc);
    ctx.device.bindBufferMemory(result.buffer, result.memory, 0);
    return result;
}

VulkanBuffer buffer_create_staging(VulkanContext& ctx,
    vk::DeviceSize size, const void* data)
{
    auto result = buffer_create(ctx,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, size);

    if (data != nullptr)
    {
        utils_copy_to_memory(ctx, result.memory, data, size);
    }
    return result;
}

VulkanBuffer buffer_create_on_device(VulkanContext& ctx, const vk::BufferUsageFlags& usageFlags, vk::DeviceSize size)
{
    return buffer_create(ctx, usageFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, size);
}

void* buffer_map(VulkanContext& ctx, VulkanBuffer& buffer, size_t offset, VkDeviceSize size)
{
    buffer.mapped = ctx.device.mapMemory(buffer.memory, offset, size, vk::MemoryMapFlags());
    return buffer.mapped;
}

} // namespace vulkan
