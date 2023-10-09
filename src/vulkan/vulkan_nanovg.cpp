/*#include <filesystem>

#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <zest/logger/logger.h>
#include <zest/time/timer.h>

#include "vklive/vulkan/vulkan_command.h"
#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_imgui.h"
#include "vklive/vulkan/vulkan_render.h"
#include "vklive/vulkan/vulkan_scene.h"
#include "vklive/vulkan/vulkan_utils.h"

#include "imgui_impl_sdl2.h"

#include "config_app.h"

#include <vklive/process/process.h>
#include <vklive/vulkan/vulkan_imgui_texture.h>
#include <vklive/vulkan/vulkan_shader.h>
*/
#define NANOVG_VULKAN_IMPLEMENTATION
#include <vklive/vulkan/nanovg_vk.h>
#include <vklive/vulkan/vulkan_buffer.h>
#include <vklive/vulkan/vulkan_command.h>
#include <zest/ui/nanovg.h>

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
    });
}

void vulkan_nanovg_draw(VulkanContext& ctx, VkRenderPass renderPass, VkCommandBuffer cmd)
{
    /*
    NVGparams* pParams = nvgInternalParams(ctx.vg);
    VKNVGcontext *vk = (VKNVGcontext *)pParams->userPtr;
    vk->createInfo.renderpass = renderPass;
    vk->createInfo.cmdBuffer = cmd;
    nvgBeginFrame(ctx.vg, (float)ctx.frameBufferSize.x, (float)ctx.frameBufferSize.y, 1.0f);

    NVGcolor col;
    col.r = 1.0f;
    col.a = 1.0f;
    nvgFillColor(ctx.vg, col);
    nvgCircle(ctx.vg, 100.0f, 100.0f, 50.0f);
    nvgFill(ctx.vg);

    nvgEndFrame(ctx, ctx.vg);
    */
}


} // namespace vulkan
