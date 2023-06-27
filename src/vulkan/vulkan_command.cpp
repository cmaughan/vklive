#include <thread>

#include <zest/logger/logger.h>

#include "vklive/vulkan/vulkan_command.h"

namespace vulkan
{
const uint64_t FenceTimeout = 100000000;

void command_submit_wait(VulkanContext& ctx, vk::Queue const& queue, vk::CommandBuffer const& commandBuffer)
{
    LOG(DBG, "Submit Wait");
    vk::Fence fence = ctx.device.createFence(vk::FenceCreateInfo());
    debug_set_fence_name(ctx.device, fence, "CommandSubmitWait::Fence");
    queue.submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &commandBuffer), fence);
    while (vk::Result::eTimeout == ctx.device.waitForFences(fence, VK_TRUE, FenceTimeout))
        ;
    ctx.device.destroyFence(fence);
}

void utils_flush_command_buffer(VulkanContext& ctx, vk::CommandBuffer& commandBuffer)
{
    if (!commandBuffer)
    {
        return;
    }
    LOG(DBG, "Flush Command Buffer");
    context_get_queue(ctx).submit(vk::SubmitInfo{ 0, nullptr, nullptr, 1, &commandBuffer }, vk::Fence());
    context_get_queue(ctx).waitIdle();
    ctx.device.waitIdle();
}

vk::CommandPool utils_get_command_pool(VulkanContext& ctx)
{
    if (!ctx.commandPool)
    {
        vk::CommandPoolCreateInfo cmdPoolInfo;
        cmdPoolInfo.queueFamilyIndex = ctx.graphicsQueue;
        cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        ctx.commandPool = ctx.device.createCommandPool(cmdPoolInfo);
        debug_set_commandpool_name(ctx.device, ctx.commandPool, "Command::CommandPool");
    }
    return ctx.commandPool;
}

vk::CommandBuffer utils_create_command_buffer(VulkanContext& ctx, vk::CommandBufferLevel level)
{
    vk::CommandBuffer cmdBuffer;
    vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
    cmdBufAllocateInfo.commandPool = utils_get_command_pool(ctx);
    cmdBufAllocateInfo.level = level;
    cmdBufAllocateInfo.commandBufferCount = 1;
    cmdBuffer = ctx.device.allocateCommandBuffers(cmdBufAllocateInfo)[0];
    return cmdBuffer;
}

// Create a short lived command buffer which is immediately executed and released
// This function is intended for initialization only.  It incurs a queue and device
// flush and may impact performance if used in non-setup code
void utils_with_command_buffer(VulkanContext& ctx, const std::function<void(const vk::CommandBuffer& commandBuffer)>& f)
{
    vk::CommandBuffer commandBuffer = utils_create_command_buffer(ctx, vk::CommandBufferLevel::ePrimary);
    commandBuffer.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    f(commandBuffer);
    commandBuffer.end();
    utils_flush_command_buffer(ctx, commandBuffer);
    ctx.device.freeCommandBuffers(utils_get_command_pool(ctx), commandBuffer);
}
}

