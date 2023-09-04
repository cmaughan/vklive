#include <SDL2/SDL.h>

#include "vklive/scene.h"

#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_device.h>
#include <vklive/vulkan/vulkan_imgui.h>
#include <vklive/vulkan/vulkan_render.h>
#include <vklive/vulkan/vulkan_scene.h>

#include <imgui_impl_sdl2.h>

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

        for (auto& [frame, cache] : ctx.descriptorCache)
        {
            vulkan_descriptor_destroy_pools(ctx, cache);
        }
    }

    ctx.descriptorCache.clear();

    vulkan::imgui_shutdown(ctx);
    vulkan::window_destroy(ctx, &ctx.mainWindowData);
    vulkan::render_destroy(ctx);
    vulkan::context_destroy(ctx);

    SDL_DestroyWindow(ctx.window);

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void VulkanDevice::InitScene(Scene& scene)
{
    vulkan::vulkan_scene_create(ctx, scene);
}

void VulkanDevice::DestroyScene(Scene& scene)
{
    auto pVulkanScene = vulkan::vulkan_scene_get(ctx, scene);
    if (pVulkanScene)
    {
        ctx.device.waitIdle();
        vulkan::vulkan_scene_destroy(ctx, *pVulkanScene);
    }
}

void VulkanDevice::ImGui_Render(ImDrawData* pDrawData)
{
    vulkan::imgui_render(ctx, &ctx.mainWindowData, pDrawData);
}

void VulkanDevice::ValidateSwapChain()
{
    vulkan::main_window_validate_swapchain(ctx);
}

void VulkanDevice::Render_3D(Scene& scene, const glm::vec2& size)
{
    vulkan::render(ctx, glm::vec4(0.0f, 0.0f, size.x, size.y), scene);
}

void VulkanDevice::ImGui_Render_3D(Scene& scene, bool backgroundRender)
{
    vulkan::imgui_render_3d(ctx, scene, backgroundRender);
    vulkan::imgui_render_targets(ctx, scene);
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
        ".vert",
        ".rchit",
        ".rgen",
        ".rmiss"
    };
}

} // namespace vulkan
