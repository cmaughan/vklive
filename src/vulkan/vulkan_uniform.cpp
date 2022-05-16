#include "vklive/vulkan/vulkan_uniform.h"

namespace vulkan
{

VulkanBuffer uniform_create(VulkanContext& ctx, vk::DeviceSize size)
{
    auto deviceProperties = ctx.physicalDevice.getProperties();
    auto alignment = deviceProperties.limits.minUniformBufferOffsetAlignment;
    auto extra = size % alignment;
    auto count = 1;
    auto alignedSize = size + (alignment - extra);
    auto allocatedSize = count * alignedSize;
    auto result = buffer_create(ctx, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, allocatedSize);
    result.alignment = alignedSize;
    result.descriptor.range = result.alignment;
    return result;
}
} // namespace vulkan
