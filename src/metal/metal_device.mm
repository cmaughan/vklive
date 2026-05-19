#include <sstream>

#import <Metal/Metal.h>

#include <SDL2/SDL.h>

#include <zest/imgui/imgui.h>
#include <zest/imgui/imgui_impl_sdl2.h>
#include <zest/logger/logger.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_device.h>
#include <vklive/metal/metal_imgui.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/scene.h>

namespace
{

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

} // namespace

namespace metal
{

std::shared_ptr<IDevice> create_metal_device(SDL_Window* pWindow, const std::string& iniPath, bool viewports)
{
    return std::static_pointer_cast<IDevice>(std::make_shared<MetalDevice>(pWindow, iniPath, viewports));
}

MetalDevice::MetalDevice(SDL_Window* pWindow, const std::string& iniPath, bool viewports)
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

    context_init(ctx);
    imgui_init(ctx, iniPath, viewports);
}

MetalDevice::~MetalDevice()
{
    context_wait_idle(ctx);
    imgui_shutdown(ctx);
    context_destroy(ctx);

    if (ctx.window)
    {
        SDL_DestroyWindow(ctx.window);
        ctx.window = nullptr;
    }

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void MetalDevice::InitScene(Scene& scene)
{
    metal_scene_create(ctx, scene);
}

void MetalDevice::DestroyScene(Scene& scene)
{
    context_wait_idle(ctx);
    metal_scene_destroy(ctx, scene);
}

void MetalDevice::ImGui_Render(ImDrawData* pDrawData)
{
    imgui_render(ctx, pDrawData);
}

void MetalDevice::ValidateSwapChain()
{
    context_validate_drawable_size(ctx);
}

RenderOutput MetalDevice::Render_3D(Scene& scene, const glm::vec2& size)
{
    auto spMetalScene = metal_scene_get(ctx, scene);
    if (!spMetalScene)
    {
        spMetalScene = metal_scene_create(ctx, scene);
    }
    if (!spMetalScene)
    {
        return {};
    }
    return metal_scene_render(ctx, *spMetalScene, size);
}

void MetalDevice::WriteToFile(Scene& scene, const fs::path& path)
{
    if ((scene.GlobalFrameCount < scene.maxRecordFrame) && scene.recording)
    {
        auto spMetalScene = metal_scene_get(ctx, scene);
        if (!spMetalScene)
        {
            spMetalScene = metal_scene_create(ctx, scene);
        }
        if (spMetalScene)
        {
            metal_scene_write_to_file(ctx, *spMetalScene, path);
        }
        else
        {
            scene.recording = false;
        }
    }
    else
    {
        scene.recording = false;
    }
}

void MetalDevice::WaitIdle()
{
    context_wait_idle(ctx);
}

void MetalDevice::Present()
{
    context_present(ctx);
}

RenderBackend MetalDevice::Backend() const
{
    return RenderBackend::Metal;
}

std::vector<RenderTargetView> MetalDevice::TargetViews(Scene& scene)
{
    auto spMetalScene = metal_scene_get(ctx, scene);
    if (!spMetalScene)
    {
        return {};
    }
    return metal_scene_target_views(ctx, *spMetalScene);
}

DeviceContext& MetalDevice::Context()
{
    return ctx;
}

std::set<std::string> MetalDevice::ShaderFileExtensions()
{
    return std::set<std::string>{
        ".frag",
        ".vert",
    };
}

std::string MetalDevice::GetDeviceString() const
{
    auto device = bridge<id<MTLDevice>>(ctx.device);

    std::ostringstream str;
    str << "Renderer: Metal" << std::endl;
    str << "Device Name: ";
    if (device)
    {
        str << [[device name] UTF8String];
    }
    else
    {
        str << "Unknown";
    }
    str << std::endl;

    return str.str();
}

} // namespace metal
