#include <SDL2/SDL.h>

#include "vklive/scene.h"

#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_scene.h>
#include <vklive/vulkan/vulkan_render.h>
#include <vklive/vulkan/vulkan_imgui.h>
#include <vklive/vulkan/vulkan_device.h>

#include <vklive/imgui/imgui_sdl.h>

// Thin wrapper around vulkan
namespace vulkan
{

std::shared_ptr<IDevice> create_vulkan_device(SDL_Window* pWindow, bool viewports)
{
    return std::static_pointer_cast<IDevice>(std::make_shared<VulkanDevice>(pWindow, viewports));
}

VulkanDevice::VulkanDevice(SDL_Window* pWindow, bool viewports)
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
    vulkan::imgui_init(ctx, viewports);
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

void VulkanDevice::InitScene(Scene& scene)
{
    vulkan::scene_init(ctx, scene);
}

void VulkanDevice::DestroyScene(Scene& scene)
{
    vulkan::scene_destroy(ctx, scene);
}

void VulkanDevice::ImGui_Render(ImDrawData* pDrawData)
{
    vulkan::imgui_render(ctx, &ctx.mainWindowData, pDrawData);
}
    
void VulkanDevice::ValidateSwapChain()
{
    vulkan::main_window_validate_swapchain(ctx);
}
    
void VulkanDevice::ImGui_Render_3D(Scene& scene, bool backgroundRender)
{
    vulkan::imgui_render_3d(ctx, scene, backgroundRender);
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

} // namespace vulkan
