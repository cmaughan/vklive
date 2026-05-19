#include <sstream>

#import <Metal/Metal.h>

#include <SDL2/SDL.h>

#include <zest/imgui/imgui.h>
#include <zest/imgui/imgui_impl_sdl2.h>
#include <zest/logger/logger.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_device.h>
#include <vklive/metal/metal_imgui.h>
#include <vklive/scene.h>
#include <vklive/validation.h>

namespace
{

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

void report_scene_error_once(Scene& scene, bool& reported, const std::string& text)
{
    if (reported)
    {
        return;
    }

    Message msg;
    msg.severity = MessageSeverity::Error;
    msg.path = scene.sceneGraphPath;
    msg.text = text;
    scene.errors.push_back(msg);
    validation_error(text);
    reported = true;
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
    report_scene_error_once(scene, m_reportedSceneUnsupported, "Metal scene rendering is not implemented yet. The Metal backend currently supports the editor and ImGui shell only.");
}

void MetalDevice::DestroyScene(Scene& scene)
{
    (void)scene;
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
    (void)size;
    report_scene_error_once(scene, m_reportedRenderUnsupported, "Metal pass rendering is not implemented yet.");
    return {};
}

void MetalDevice::WriteToFile(Scene& scene, const fs::path& path)
{
    (void)path;
    if ((scene.GlobalFrameCount < scene.maxRecordFrame) && scene.recording)
    {
        report_scene_error_once(scene, m_reportedWriteUnsupported, "Metal render capture is not implemented yet.");
    }
    scene.recording = false;
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
    (void)scene;
    return {};
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
