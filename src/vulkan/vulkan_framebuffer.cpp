#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{
void framebuffer_destroy(VulkanContext& ctx, VulkanFrameBuffer& frame)
{
    if (frame.framebuffer)
    {
        ctx.device.destroyFramebuffer(frame.framebuffer);
        frame.framebuffer = nullptr;
    }
}

void framebuffer_create(VulkanContext& ctx, VulkanFrameBuffer& frame, const std::vector<VulkanSurface*>& colorBuffers, VulkanSurface* pDepth, const vk::RenderPass& renderPass)
{
    framebuffer_destroy(ctx, frame);

    auto attachments = std::vector<vk::ImageView>{ colorBuffers[0]->view };
    if (pDepth)
    {
        attachments.push_back(pDepth->view);
    }

    vk::FramebufferCreateInfo fbufCreateInfo;
    fbufCreateInfo.renderPass = renderPass;
    fbufCreateInfo.attachmentCount = (uint32_t)attachments.size();
    fbufCreateInfo.pAttachments = attachments.data();
    fbufCreateInfo.width = colorBuffers[0]->extent.width;
    fbufCreateInfo.height = colorBuffers[0]->extent.height;
    fbufCreateInfo.layers = 1;
    frame.framebuffer = ctx.device.createFramebuffer(fbufCreateInfo);
}
} // namespace vulkan
