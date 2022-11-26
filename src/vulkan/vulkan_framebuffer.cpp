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

    // Add the attachments with the colors first, then the depth?
    // Target ordering is enforced
    std::vector<vk::ImageView> attachments;
    vk::ImageView depthView;
    for (auto& pTargetData : passTargets.orderedTargets)
    {
        if (vulkan_format_is_depth(pTargetData->pVulkanSurface->format))
        {
            depthView = pTargetData->pVulkanSurface->view;
        }
        else
        {
            attachments.emplace_back(pTargetData->pVulkanSurface->view);
        }
    }

    if (depthView)
    {
        attachments.push_back(depthView);
    }

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
