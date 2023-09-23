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

RenderOutput VulkanDevice::Render_3D(Scene& scene, const glm::vec2& size)
{
    vulkan::render(ctx, glm::vec4(0.0f, 0.0f, size.x, size.y), scene);
    return vulkan::render_get_output(ctx, scene);
}

void VulkanDevice::WriteToFile(Scene& scene, const fs::path& path)
{
    if (scene.GlobalFrameCount < 10)
    {
        vulkan::render_write_output(ctx, scene, path);
    }

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

std::string VulkanDevice::GetDeviceString() const
{
    std::ostringstream str;

    str << "API Version: " << ctx.physicalDevice.getProperties().apiVersion << std::endl;
    str << "Device Name: " << ctx.physicalDevice.getProperties().deviceName << std::endl;

    str << "Device Extensions:" << std::endl;
    for (auto& ext : ctx.supportedDeviceExtensions)
    {
        str << ext.extensionName << std::endl;
    }

    str << std::endl
        << "Layer Properties:" << std::endl;
    for (auto& ext : ctx.supportedInstancelayerProperties)
    {
        str << ext.layerName << std::endl;
    }

    str << std::endl
        << "Instance Extensions:" << std::endl;
    for (auto& ext : ctx.supportedInstanceExtensions)
    {
        str << ext.extensionName << std::endl;
    }

    return str.str();
}

} // namespace vulkan
