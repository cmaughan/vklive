#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_pass.h"
#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{
void framebuffer_destroy(VulkanContext& ctx, vk::Framebuffer& frameBuffer)
{
    ctx.device.destroyFramebuffer(frameBuffer);
}

void vulkan_framebuffer_create(VulkanContext& ctx, vk::Framebuffer& frameBuffer, const VulkanPassTargets& passTargets, const vk::RenderPass& renderPass)
{
    framebuffer_destroy(ctx, frameBuffer);

    std::vector<vk::ImageView> attachments;
    for (auto& [name, pSurface] : passTargets.targets)
    {
        attachments.emplace_back(pSurface->view);
    }

    /*
    if (passTargets.depth)
    {
        attachments.push_back(passTargets.depth->view);
    }
    */

    assert(!attachments.empty() && passTargets.targetSize.x != 0 && passTargets.targetSize.y != 0);
    vk::FramebufferCreateInfo fbufCreateInfo;
    fbufCreateInfo.renderPass = renderPass;
    fbufCreateInfo.attachmentCount = (uint32_t)attachments.size();
    fbufCreateInfo.pAttachments = attachments.data();
    fbufCreateInfo.width = passTargets.targetSize.x;
    fbufCreateInfo.height = passTargets.targetSize.y;
    fbufCreateInfo.layers = 1;
    frameBuffer = ctx.device.createFramebuffer(fbufCreateInfo);
}
} // namespace vulkan
