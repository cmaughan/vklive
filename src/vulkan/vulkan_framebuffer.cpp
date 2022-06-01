#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_utils.h"
#include "vklive/vulkan/vulkan_scene.h"

namespace vulkan
{

void vulkan_framebuffer_destroy(VulkanContext& ctx, VulkanFrameBuffer* pFrameBuffer)
{
    if (pFrameBuffer && pFrameBuffer->frameBuffer)
    {
        ctx.device.destroyFramebuffer(pFrameBuffer->frameBuffer);
        pFrameBuffer->frameBuffer = nullptr;
    }
}

IDeviceFrameBuffer* vulkan_framebuffer_create(VulkanContext& ctx, SceneGraph& scene, VulkanRenderPass& renderPass)
{
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    VulkanFrameBuffer* pVulkanFrameBuffer = nullptr;

    auto itr = pVulkanScene->frameBuffers.find(renderPass.pPass->name);
    if (itr != pVulkanScene->frameBuffers.end())
    {
        return static_cast<VulkanFrameBuffer*>(itr->second.get()); 

    }
    else
    {
        auto spFrameBuffer = std::make_shared<VulkanFrameBuffer>(&renderPass);
        pVulkanScene->frameBuffers[renderPass.pPass->name] = spFrameBuffer;
        pVulkanFrameBuffer = spFrameBuffer.get();
    }

    auto attachments = std::vector<vk::ImageView>();
    for (auto& image : renderPass.colorImages)
    {
        attachments.push_back(image->view);
    }
    
    if (renderPass.pDepthImage)
    {
        attachments.push_back(renderPass.pDepthImage->view);
    }

    vk::FramebufferCreateInfo fbufCreateInfo;
    fbufCreateInfo.renderPass = renderPass.renderPass;
    fbufCreateInfo.attachmentCount = (uint32_t)attachments.size();
    fbufCreateInfo.pAttachments = attachments.data();
    fbufCreateInfo.width = renderPass.targetSize.x;
    fbufCreateInfo.height = renderPass.targetSize.y;
    fbufCreateInfo.layers = 1;
    pVulkanFrameBuffer->frameBuffer = ctx.device.createFramebuffer(fbufCreateInfo);

    debug_set_framebuffer_name(ctx.device, pVulkanFrameBuffer->frameBuffer, debug_pass_name(renderPass, "FrameBuffer"));
    return pVulkanFrameBuffer;
}
} // namespace vulkan
