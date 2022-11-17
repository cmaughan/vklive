#include <cassert>
#include <iostream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vklive/logger/logger.h>

#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_utils.h"
#include "vklive/vulkan/vulkan_framebuffer.h"

//#define IMGUI_UNLIMITED_FRAME_RATE

namespace vulkan
{
namespace
{
// Also destroy old swap chain and in-flight frames data, if any.
void CreateWindowSwapChain(VulkanContext& ctx, VulkanWindow* wd, int w, int h)
{
    vk::Result err;
    vk::SwapchainKHR old_swapchain = wd->swapchain;
    wd->swapchain = nullptr;
    ctx.device.waitIdle();

    // We don't use ImGui_ImplVulkanH_DestroyWindow() because we want to preserve the old swapchain to create the new one.
    // audio_destroy old Framebuffer
    for (uint32_t i = 0; i < wd->imageCount; i++)
    {
        window_destroy_frame(ctx, &wd->frames[i]);
        window_destroy_frame_semaphores(ctx, &wd->frameSemaphores[i]);
    }
    free(wd->frames);
    free(wd->frameSemaphores);
    wd->frames = NULL;
    wd->frameSemaphores = NULL;
    wd->imageCount = 0;
    if (wd->renderPass)
    {
        ctx.device.destroyRenderPass(wd->renderPass);
    }

    if (wd->pipeline)
    {
        ctx.device.destroyPipeline(wd->pipeline);
    }

    // If min image count was not specified, request different count of images dependent on selected present mode
    if (ctx.minImageCount == 0)
    {
        ctx.minImageCount = utils_get_min_image_count_from_present_mode(wd->presentMode);
    }

    wd->imageCount = ctx.minImageCount;
    // Create Swapchain
    {
        vk::SwapchainCreateInfoKHR info;
        info.surface = wd->surface;
        info.minImageCount = ctx.minImageCount;
        info.imageFormat = wd->surfaceFormat.format;
        info.imageColorSpace = wd->surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        info.imageSharingMode = vk::SharingMode::eExclusive; // Assume that graphics family == present family
        info.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
        info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode = wd->presentMode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        vk::SurfaceCapabilitiesKHR cap = ctx.physicalDevice.getSurfaceCapabilitiesKHR(wd->surface);
        if (info.minImageCount < cap.minImageCount)
        {
            info.minImageCount = cap.minImageCount;
        }
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
        {
            info.minImageCount = cap.maxImageCount;
        }

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent.width = wd->width = w;
            info.imageExtent.height = wd->height = h;
        }
        else
        {
            info.imageExtent.width = wd->width = cap.currentExtent.width;
            info.imageExtent.height = wd->height = cap.currentExtent.height;
        }
        wd->swapchain = ctx.device.createSwapchainKHR(info);
        debug_set_swapchain_name(ctx.device, wd->swapchain, "Window::SwapChain");

        auto swapImages = ctx.device.getSwapchainImagesKHR(wd->swapchain);

        assert(wd->frames == NULL);
        wd->frames = (VulkanSwapFrame*)malloc(sizeof(VulkanSwapFrame) * wd->imageCount);
        wd->frameSemaphores = (VulkanFrameSemaphores*)malloc(sizeof(VulkanFrameSemaphores) * wd->imageCount);
        memset(wd->frames, 0, sizeof(wd->frames[0]) * wd->imageCount);
        memset(wd->frameSemaphores, 0, sizeof(wd->frameSemaphores[0]) * wd->imageCount);
        for (uint32_t i = 0; i < wd->imageCount; i++)
        {
            VulkanSurface img(nullptr);
            img.image = swapImages[i];
            wd->frames[i].colorBuffers.push_back(img);
        
            debug_set_image_name(ctx.device, swapImages[i], std::string("Window::SwapChain_Image:") + std::to_string(i) );
        }
    }

    if (old_swapchain)
    {
        ctx.device.destroySwapchainKHR(old_swapchain);
    }

    // Create the Render Pass
    {
        vk::AttachmentDescription attachment;
        attachment.format = wd->surfaceFormat.format;
        attachment.samples = vk::SampleCountFlagBits::e1;
        attachment.loadOp = wd->clearEnable ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
        attachment.storeOp = vk::AttachmentStoreOp::eStore;
        attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachment.initialLayout = vk::ImageLayout::eUndefined;
        attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference color_attachment;
        color_attachment.attachment = 0;
        color_attachment.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;

        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        wd->renderPass = ctx.device.createRenderPass(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), attachment, subpass, dependency));
        debug_set_renderpass_name(ctx.device, wd->renderPass, "Window::SwapChain_RenderPass");
    }

    // Create The Image Views
    {
        vk::ImageViewCreateInfo info = {};
        info.viewType = vk::ImageViewType::e2D;
        info.format = wd->surfaceFormat.format;
        info.components.r = vk::ComponentSwizzle::eR;
        info.components.g = vk::ComponentSwizzle::eG;
        info.components.b = vk::ComponentSwizzle::eB;
        info.components.a = vk::ComponentSwizzle::eA;
        vk::ImageSubresourceRange image_range = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (uint32_t i = 0; i < wd->imageCount; i++)
        {
            VulkanSwapFrame* fd = &wd->frames[i];
            info.image = fd->colorBuffers[0].image;
            fd->colorBuffers[0].view = ctx.device.createImageView(info);
            
            debug_set_imageview_name(ctx.device, fd->colorBuffers[0].view, std::string("Window::SwapChain_ImageView_") + std::to_string(i));
        }
    }

    // Create Framebuffer
    {
        vk::ImageView attachment[1];
        vk::FramebufferCreateInfo info;
        info.renderPass = wd->renderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.width = wd->width;
        info.height = wd->height;
        info.layers = 1;
        for (uint32_t i = 0; i < wd->imageCount; i++)
        {
            VulkanSwapFrame* fd = &wd->frames[i];
            attachment[0] = fd->colorBuffers[0].view;
            fd->framebuffer = ctx.device.createFramebuffer(info);
            debug_set_framebuffer_name(ctx.device, fd->framebuffer, std::string("Window::SwapChain_Framebuffer:") + std::to_string(i));
        }
    }
}

// Create command buffers & semaphores for a given window
void CreateWindowCommandBuffers(VulkanContext& ctx, VulkanWindow* wd)
{
    assert(ctx.physicalDevice && ctx.device);

    // Create Command Buffers
    for (uint32_t i = 0; i < wd->imageCount; i++)
    {
        VulkanSwapFrame* fd = &wd->frames[i];
        VulkanFrameSemaphores* fsd = &wd->frameSemaphores[i];
        fd->commandPool = ctx.device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer));
        fd->commandBuffer = ctx.device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(fd->commandPool, vk::CommandBufferLevel::ePrimary, 1))[0];
        
        debug_set_commandbuffer_name(ctx.device, fd->commandBuffer, std::string("VulkanWindow::CommandBuffer:") + std::to_string(i));
        debug_set_commandpool_name(ctx.device, fd->commandPool, std::string("VulkanWindow::CommandPool:") + std::to_string(i));

        // fd->CommandBuffer.insertDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT("My Command Buffer"));
        fd->fence = ctx.device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
        
        debug_set_fence_name(ctx.device, fd->fence, std::string("VulkanWindow::Fence:") + std::to_string(i));

        fsd->imageAcquiredSemaphore = ctx.device.createSemaphore(vk::SemaphoreCreateInfo());
        fsd->renderCompleteSemaphore = ctx.device.createSemaphore(vk::SemaphoreCreateInfo());
        
        debug_set_semaphore_name(ctx.device, fsd->imageAcquiredSemaphore, std::string("VulkanWindow::ImageAcquiredSemaphore:") + std::to_string(i));
        debug_set_semaphore_name(ctx.device, fsd->renderCompleteSemaphore, std::string("VulkanWindow::RenderCompleteSemaphore:") + std::to_string(i));
    }
}

} // namespace

// Create the main window
bool main_window_init(VulkanContext& ctx)
{
    auto wd = &ctx.mainWindowData;

    // Create ctx.window Surface
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(ctx.window, ctx.instance, &surface) == 0)
    {
        std::cout << "Failed to create Vulkan surface.\n";
        return false;
    }
    debug_set_surface_name(ctx.device, surface, "MainWindow::Surface");

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSize(ctx.window, &w, &h);

    wd->surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physicalDevice, ctx.graphicsQueue, wd->surface, &res);
    if (res != VK_TRUE)
    {
        LOG(DBG, "Error no WSI support on physical device 0");
        return false;
    }

    // Select Surface Format
    std::vector<vk::Format> requestSurfaceImageFormat = { vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm };
    const vk::ColorSpaceKHR requestSurfaceColorSpace = vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT;
    wd->surfaceFormat = utils_select_surface_format(ctx, wd->surface, requestSurfaceImageFormat, requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
    auto present_modes = std::vector<vk::PresentModeKHR>{ vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifo };
#else
    auto present_modes = std::vector<vk::PresentModeKHR>{ vk::PresentModeKHR::eFifo };
#endif
    wd->presentMode = utils_select_present_mode(ctx, wd->surface, present_modes);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    window_create_or_resize(ctx, wd, w, h);

    return true;
}

// Ensure the swap chain is valid for the main window
void main_window_validate_swapchain(VulkanContext& ctx)
{
    // Resize swap chain?
    if (!ctx.swapChainRebuild)
    {
        return;
    }

    int width, height;
    SDL_GetWindowSize(ctx.window, &width, &height);
    if (width > 0 && height > 0)
    {
        if (ctx.mainWindowData.imageCount != ctx.minImageCount)
        {
            ctx.device.waitIdle();

            // ImGui dependency here that needs breaking
            extern void imgui_viewport_destroy_all(VulkanContext&);
            imgui_viewport_destroy_all(ctx);

            ctx.mainWindowData.imageCount = ctx.minImageCount;
        }

        window_create_or_resize(ctx, &ctx.mainWindowData, width, height);
        ctx.mainWindowData.frameIndex = 0;
        ctx.swapChainRebuild = false;
    }
}

// swap the main window
void main_window_present(VulkanContext& ctx)
{
    if (ctx.swapChainRebuild)
    {
        return;
    }

    auto wnd = &ctx.mainWindowData;
    vk::Semaphore render_complete_semaphore = wnd->frameSemaphores[wnd->semaphoreIndex].renderCompleteSemaphore;
    auto info = vk::PresentInfoKHR(1, &render_complete_semaphore, 1, &wnd->swapchain, &wnd->frameIndex);
    vk::Result err = ctx.queue.presentKHR(&info);
    if (err == vk::Result::eErrorOutOfDateKHR || err == vk::Result::eSuboptimalKHR)
    {
        ctx.swapChainRebuild = true;
        return;
    }
    wnd->semaphoreIndex = (wnd->semaphoreIndex + 1) % wnd->imageCount; // Now we can use the next set of semaphores
}

void window_destroy(VulkanContext& ctx, VulkanWindow* wd)
{
    vkDeviceWaitIdle(ctx.device); // FIXME: We could wait on the Queue if we had the queue in wd-> (otherwise VulkanH functions can't use globals)

    for (uint32_t i = 0; i < wd->imageCount; i++)
    {
        window_destroy_frame(ctx, &wd->frames[i]);
        window_destroy_frame_semaphores(ctx, &wd->frameSemaphores[i]);
    }
    free(wd->frames);
    free(wd->frameSemaphores);
    wd->frames = NULL;
    wd->frameSemaphores = NULL;
    ctx.device.destroyPipeline(wd->pipeline);
    ctx.device.destroyRenderPass(wd->renderPass);
    ctx.device.destroySwapchainKHR(wd->swapchain);
    ctx.instance.destroySurfaceKHR(wd->surface);

    wd->pipeline = nullptr;
    wd->renderPass = nullptr;
    wd->swapchain = nullptr;
    wd->surface = nullptr;

    *wd = VulkanWindow();
}

void window_destroy_frame(VulkanContext& ctx, VulkanSwapFrame* fd)
{
    ctx.device.destroyFence(fd->fence);
    ctx.device.freeCommandBuffers(fd->commandPool, { fd->commandBuffer });
    ctx.device.destroyCommandPool(fd->commandPool);

    fd->fence = nullptr;
    fd->commandBuffer = nullptr;
    fd->commandPool = nullptr;

    for (auto& buffer : fd->colorBuffers)
    {
        ctx.device.destroyImageView(buffer.view);
        buffer.view = nullptr;
    }
    fd->colorBuffers.clear();
    ctx.device.destroyFramebuffer(fd->framebuffer);
    fd->framebuffer = nullptr;
}

void window_destroy_frame_semaphores(VulkanContext& ctx, VulkanFrameSemaphores* fsd)
{
    ctx.device.destroySemaphore(fsd->imageAcquiredSemaphore);
    ctx.device.destroySemaphore(fsd->renderCompleteSemaphore);
    fsd->imageAcquiredSemaphore = fsd->renderCompleteSemaphore = nullptr;
}

void window_destroy_frame_renderbuffers(VulkanContext& ctx, FrameRenderBuffers* buffers)
{
    ctx.device.destroyBuffer(buffers->vertexBuffer);
    ctx.device.destroyBuffer(buffers->indexBuffer);
    ctx.device.freeMemory(buffers->vertexBufferMemory);
    ctx.device.freeMemory(buffers->indexBufferMemory);
    buffers->vertexBufferSize = 0;
    buffers->indexBufferSize = 0;
    buffers->vertexBuffer = nullptr;
    buffers->indexBuffer = nullptr;
    buffers->vertexBufferMemory = nullptr;
    buffers->indexBufferMemory = nullptr;
}

void window_destroy_renderbuffers(VulkanContext& ctx, WindowRenderBuffers* buffers)
{
    for (uint32_t n = 0; n < buffers->count; n++)
    {
        window_destroy_frame_renderbuffers(ctx, &buffers->frameRenderBuffers[n]);
    }
    free(buffers->frameRenderBuffers);
    buffers->frameRenderBuffers = NULL;
    buffers->index = 0;
    buffers->count = 0;
}

// Create or resize window; could be main or extra platform window
void window_create_or_resize(VulkanContext& ctx, VulkanWindow* wd, int width, int height)
{
    CreateWindowSwapChain(ctx, wd, width, height);
    CreateWindowCommandBuffers(ctx, wd);
}
} // namespace vulkan
