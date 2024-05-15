#define NANOVG_VULKAN_IMPLEMENTATION
#include <vklive/vulkan/nanovg_vk.h>
#include <vklive/vulkan/vulkan_buffer.h>
#include <vklive/vulkan/vulkan_command.h>
#include <vklive/vulkan/vulkan_scene.h>
#include <vklive/vulkan/vulkan_pass.h>
#include <zest/ui/nanovg.h>
#include <zest/file/runtree.h>

namespace vulkan
{

void vulkan_nanovg_init(VulkanContext& ctx)
{
    VKNVGCreateInfo createInfo = { 0 };
    createInfo.device = ctx.device;
    createInfo.gpu = ctx.physicalDevice;
    createInfo.swapchainImageCount = ctx.mainWindowData.imageCount;
    createInfo.currentFrame = &ctx.mainWindowData.frameIndex;

    uint32_t flags = NVG_ANTIALIAS;
/* #endif
#if DEMO_STENCIL_STROKES
  flags |= NVG_STENCIL_STROKES;
#endif
*/
    utils_with_command_buffer(ctx, [&](auto cmd) {
        createInfo.cmdBuffer = cmd;
        ctx.vg = nvgCreateVk(createInfo, flags, ctx.queue);


        auto fontPath = Zest::runtree_find_path("fonts/Roboto-Regular.ttf");
        ctx.defaultFont = nvgCreateFont(ctx.vg, "sans", fontPath.string().c_str());
    });
}

void vulkan_nanovg_begin(VulkanContext& ctx, VulkanPass& vulkanPass, vk::CommandBuffer& cmd)
{
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(ctx, passFrameData);

    NVGparams* pParams = nvgInternalParams(ctx.vg);
    VKNVGcontext* vk = (VKNVGcontext*)pParams->userPtr;

    //vk->createInfo.renderpass = passTargets.renderPass;
    vk->createInfo.cmdBuffer = cmd;

    nvgBeginFrame(ctx.vg, (float)passTargets.targetSize.x, (float)passTargets.targetSize.y, 1.0f);
}

void vulkan_nanovg_end(VulkanContext& ctx)
{
    nvgEndFrame(ctx, ctx.vg);
}

} // namespace vulkan
