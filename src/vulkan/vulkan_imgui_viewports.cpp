#include <zest/logger/logger.h>

#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_imgui.h"
#include "vklive/vulkan/vulkan_utils.h"
#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_debug.h"

#include <stdio.h>
#include <iostream>

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning(disable : 4127) // condition expression is constant
#endif

namespace vulkan
{
namespace
{
void check_vk_result(vk::Result err)
{
    if (err == vk::Result::eSuccess)
    {
        return;
    }

    LOG(DBG, "[vulkan] Error: VkResult = " << err);

    if (int(err) < 0)
    {
        abort();
    }
}
} // namespace

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not tested and probably dysfunctional in this backend.
VulkanContext& GetVulkanContext()
{
    return *(VulkanContext*)ImGui::GetIO().BackendRendererUserData;
}

void imgui_viewport_create(ImGuiViewport* viewport)
{
    VulkanContext& ctx = GetVulkanContext();
    ImGuiViewportData* vd = IM_NEW(ImGuiViewportData)();
    viewport->RendererUserData = vd;
    VulkanWindow* wd = &vd->window;

    // Create surface
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    vk::Result err = (vk::Result)platform_io.Platform_CreateVkSurface(viewport, (ImU64)(VkInstance)ctx.instance, nullptr, (ImU64*)&wd->surface);
    debug_set_surface_name(ctx.device, wd->surface, "Viewport::Surface");

    check_vk_result(err);

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physicalDevice, ctx.graphicsQueue, wd->surface, &res);
    if (res != VK_TRUE)
    {
        IM_ASSERT(0); // Error: no WSI support on physical device
        return;
    }

    // Select Surface Format
    std::vector<vk::Format> requestSurfaceImageFormat = { vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm };
    const vk::ColorSpaceKHR requestSurfaceColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    wd->surfaceFormat = utils_select_surface_format(ctx, wd->surface, requestSurfaceImageFormat, requestSurfaceColorSpace);

    // Select Present Mode
    // FIXME-VULKAN: Even thought mailbox seems to get us maximum framerate with a single window, it halves framerate with a second window etc. (w/ Nvidia and SDK 1.82.1)
    auto present_modes = std::vector<vk::PresentModeKHR>{ vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifo };
    //auto present_modes = std::vector<vk::PresentModeKHR>{ vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifo };
    wd->presentMode = utils_select_present_mode(ctx, wd->surface, present_modes);
    // printf("[vulkan] Secondary window selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    wd->clearEnable = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? false : true;
    window_create_or_resize(ctx, wd, (int)viewport->Size.x, (int)viewport->Size.y);
    vd->windowOwned = true;
}

void imgui_viewport_destroy(ImGuiViewport* viewport)
{
    // The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
    VulkanContext& ctx = GetVulkanContext();
    if (ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData)
    {
        if (vd->windowOwned)
        {
            window_destroy(ctx, &vd->window);
        }
        window_destroy_renderbuffers(ctx, &vd->renderBuffers);
        IM_DELETE(vd);
    }
    viewport->RendererUserData = NULL;
}

void imgui_viewport_set_size(ImGuiViewport* viewport, ImVec2 size)
{
    VulkanContext& ctx = GetVulkanContext();
    ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData;
    if (vd == NULL) // This is NULL for the main viewport (which is left to the user/app to handle)
        return;
    vd->window.clearEnable = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? false : true;
    window_create_or_resize(ctx, &vd->window, (int)size.x, (int)size.y);
}

void imgui_viewport_render(ImGuiViewport* viewport, void*)
{
    LOG(INFO, "ImGui Viewport Render: " << std::this_thread::get_id());
    VulkanContext& ctx = GetVulkanContext();
    ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData;
    VulkanWindow* wd = &vd->window;
    VkResult err;

    VulkanSwapFrame* fd = &wd->frames[wd->frameIndex];
    VulkanFrameSemaphores* fsd = &wd->frameSemaphores[wd->semaphoreIndex];
    {
        // Get access to the new render image
        auto ret = ctx.device.acquireNextImageKHR(wd->swapchain, UINT64_MAX, fsd->imageAcquiredSemaphore);
        wd->frameIndex = ret.value;
        fd = &wd->frames[wd->frameIndex];

        // Wait for the frame
        for (;;)
        {
            auto err = ctx.device.waitForFences(fd->fence, 1, 100);
            if (err == vk::Result::eSuccess)
                break;
            if (err == vk::Result::eTimeout)
                continue;
        }

        // Begin the buffer
        ctx.device.resetCommandPool(fd->commandPool, vk::CommandPoolResetFlags());
        fd->commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        // Begin the pass
        ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        memcpy(&wd->clearValue.color.float32[0], &clear_color, 4 * sizeof(float));
        auto clearValue = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? std::vector<vk::ClearValue>{ wd->clearValue } : std::vector<vk::ClearValue>();
        fd->commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(wd->renderPass, fd->framebuffer, vk::Rect2D({ 0, 0 }, { (uint32_t)wd->width, (uint32_t)wd->height }), clearValue), vk::SubpassContents::eInline);
    }

    imgui_render_drawdata(ctx, viewport->DrawData, fd->commandBuffer, wd->pipeline);

    fd->commandBuffer.endRenderPass();

    fd->commandBuffer.end();

    ctx.device.resetFences(fd->fence);

    LOG(DBG, "Submit ImGui Viewport");
    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    context_get_queue(ctx).submit(vk::SubmitInfo(fsd->imageAcquiredSemaphore, wait_stage, fd->commandBuffer, fsd->renderCompleteSemaphore), fd->fence);
}

void imgui_viewport_swap_buffers(ImGuiViewport* viewport, void*)
{
    VulkanContext& ctx = GetVulkanContext();
    ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData;
    VulkanWindow* wd = &vd->window;

    uint32_t present_index = wd->frameIndex;

    VulkanFrameSemaphores* fsd = &wd->frameSemaphores[wd->semaphoreIndex];
    auto result = context_get_queue(ctx).presentKHR(vk::PresentInfoKHR(fsd->renderCompleteSemaphore, wd->swapchain, present_index));

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        window_create_or_resize(ctx, &vd->window, (int)viewport->Size.x, (int)viewport->Size.y);
    }
    else
    {
        check_vk_result(result);
    }

    wd->frameIndex = (wd->frameIndex + 1) % wd->imageCount; // This is for the next vkWaitForFences()
    wd->semaphoreIndex = (wd->semaphoreIndex + 1) % wd->semaphoreCount; // Now we can use the next set of semaphores
}

void imgui_viewport_destroy_all(VulkanContext& ctx)
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    for (int n = 0; n < platform_io.Viewports.Size; n++)
    {
        if (ImGuiViewportData* vd = (ImGuiViewportData*)platform_io.Viewports[n]->RendererUserData)
        {
            window_destroy_renderbuffers(ctx, &vd->renderBuffers);
        }
    }
}
} // namespace vulkan
