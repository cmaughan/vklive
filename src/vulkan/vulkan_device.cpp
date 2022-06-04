#include <SDL2/SDL.h>

#include "vklive/scene.h"

#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_device.h>
#include <vklive/vulkan/vulkan_imgui.h>
#include <vklive/vulkan/vulkan_render.h>
#include <vklive/vulkan/vulkan_scene.h>

#include <vklive/imgui/imgui_sdl.h>

// Thin wrapper around vulkan
namespace vulkan
{

std::shared_ptr<IDevice> create_vulkan_device(SDL_Window* pWindow, const std::string& iniPath, bool viewports)
{
    return std::static_pointer_cast<IDevice>(std::make_shared<VulkanDevice>(pWindow, iniPath, viewports));
}

VulkanDevice::VulkanDevice(SDL_Window* pWindow, const std::string& iniPath, bool viewports)
    : IDevice()
{
    ctx.window = pWindow;

    float ddpi;
    auto dpi = SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(pWindow), &ddpi, &ctx.hdpi, &ctx.vdpi);
    if (dpi)
    {
        ctx.vdpi = 1.0f;
    }
    else
    {
        ctx.vdpi = ctx.vdpi / 96.0f;
    }

    vulkan::context_init(ctx);
    vulkan::main_window_init(ctx);
    vulkan::render_init(ctx);
    vulkan::imgui_init(ctx, iniPath, viewports);
}

VulkanDevice::~VulkanDevice()
{
    if (!ctx.device)
    {
        return;
    }

    if (ctx.deviceState == DeviceState::Normal)
    {
        ctx.device.waitIdle();
    }

    vulkan::imgui_shutdown(ctx);
    vulkan::window_destroy(ctx, &ctx.mainWindowData);
    vulkan::render_destroy(ctx);
    vulkan::context_destroy(ctx);

    SDL_DestroyWindow(ctx.window);

    ImGui_SDL2_Shutdown();
    ImGui::DestroyContext();
}

void VulkanDevice::InitScene(SceneGraph& scene)
{
    vulkan::vulkan_scene_init(ctx, scene);
}

void VulkanDevice::DestroyScene(SceneGraph& scene)
{
    vulkan::vulkan_scene_destroy(ctx, scene);
}

void VulkanDevice::ImGui_Render(ImDrawData* pDrawData)
{
    vulkan::imgui_render(ctx, &ctx.mainWindowData, pDrawData);
}

void VulkanDevice::ValidateSwapChain()
{
    vulkan::main_window_validate_swapchain(ctx);
}

void VulkanDevice::ImGui_Render_3D(SceneGraph& scene, bool backgroundRender, const std::function<IDeviceSurface*(const glm::vec2&)>& fnDrawScene)
{
    vulkan::imgui_render_3d(ctx, scene, backgroundRender, fnDrawScene);
}

void VulkanDevice::WaitIdle()
{
    if (ctx.device)
    {
        ctx.device.waitIdle();
    }
}

void VulkanDevice::Present()
{
    vulkan::main_window_present(ctx);
}

DeviceContext& VulkanDevice::Context()
{
    return ctx;
}

std::set<std::string> VulkanDevice::ShaderFileExtensions()
{
    return std::set<std::string>{
        ".fs",
        ".gs",
        ".vs",
        ".frag",
        ".geom",
        ".vert"
    };
}

IDeviceSurface* VulkanDevice::FindSurface(const std::string& name) const
{
    auto itr = ctx.mapDeviceSurfaces.find(name);
    if (itr == ctx.mapDeviceSurfaces.end())
    {
        return nullptr;
    }
    return itr->second.get();
}
                
IDeviceSurface* VulkanDevice::AddOrUpdateSurface(Surface& surface)
{
    VulkanSurface* pVulkanSurface = nullptr;

    auto itr = ctx.mapDeviceSurfaces.find(surface.name);
    if (itr != ctx.mapDeviceSurfaces.end())
    {
        pVulkanSurface = static_cast<VulkanSurface*>(itr->second.get()); 
    }
    else
    {
        auto spSurface = std::make_shared<VulkanSurface>(&surface);
        ctx.mapDeviceSurfaces[surface.name] = spSurface;
        pVulkanSurface = spSurface.get();
    }

    if (pVulkanSurface->currentSize == surface.currentSize)
    {
        return pVulkanSurface; 
    }

    pVulkanSurface->currentSize = surface.currentSize;

    // Destroy old if necessary
    image_destroy(ctx, pVulkanSurface->image);

    if (format_is_depth(surface.format))
    {
        image_create_depth(ctx, pVulkanSurface->image, surface.currentSize, vulkan_scene_format_to_vulkan(surface.format), true, surface.name);
    }
    else
    {
        image_create(ctx, pVulkanSurface->image, surface.currentSize, vulkan_scene_format_to_vulkan(surface.format), true, surface.name);
        
        pVulkanSurface->image.sampler = ctx.device.createSampler(vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear));

        debug_set_sampler_name(ctx.device, pVulkanSurface->image.sampler, surface.name + "::Sampler");
    }

    // Temporary to ensure we can sample the backbuffer after rendering
    if (surface.name == "default_color")
    {
        image_set_sampling(ctx, pVulkanSurface->image);
        debug_set_descriptorsetlayout_name(ctx.device, pVulkanSurface->image.samplerDescriptorSetLayout, "RenderColorBuffer::DescriptorSetLayout");
        debug_set_descriptorset_name(ctx.device, pVulkanSurface->image.samplerDescriptorSet, "RenderColorBuffer::DescriptorSet");
        debug_set_sampler_name(ctx.device, pVulkanSurface->image.sampler, "RenderColorBuffer::Sampler");
    }
    return pVulkanSurface;
}

IDeviceRenderPass* VulkanDevice::AddOrUpdateRenderPass(SceneGraph& scene, Pass& pass, const std::vector<IDeviceSurface*>& targets, IDeviceSurface* pDepth)
{
    return vulkan_scene_create_renderpass(ctx, scene, pass, targets, pDepth);
}
    
IDeviceFrameBuffer* VulkanDevice::AddOrUpdateFrameBuffer(SceneGraph& scene, IDeviceRenderPass* pRenderPass)
{
    return vulkan_framebuffer_create(ctx, scene, *static_cast<VulkanRenderPass*>(pRenderPass));
}


/*
if (!pVulkanScene->commandBuffer)
    {
        pVulkanScene->fence = ctx.device.createFence(vk::FenceCreateInfo());
        pVulkanScene->commandPool = ctx.device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, ctx.graphicsQueue));
        pVulkanScene->commandBuffer = ctx.device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pVulkanScene->commandPool, vk::CommandBufferLevel::ePrimary, 1))[0];

        debug_set_commandpool_name(ctx.device, pVulkanScene->commandPool, "Scene:CommandPool");
        debug_set_commandbuffer_name(ctx.device, pVulkanScene->commandBuffer, "Scene:CommandBuffer");
        debug_set_fence_name(ctx.device, pVulkanScene->fence, "Scene:Fence");
    }
    */


void VulkanDevice::DestroySurface(const Surface& surface)
{
    auto pDeviceSurface = static_cast<VulkanSurface*>(FindSurface(surface.name));
    if (pDeviceSurface)
    {
        image_destroy(ctx, pDeviceSurface->image);
    }
}
    

} // namespace vulkan
