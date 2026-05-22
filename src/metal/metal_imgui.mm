#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <stdexcept>

#include <SDL2/SDL.h>

#include <zest/file/runtree.h>
#include <zest/imgui/imgui.h>
#include <zest/imgui/imgui_impl_sdl2.h>
#include <zest/ui/fonts.h>

#include "imgui_impl_metal_1917.h"
#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_imgui.h>
#include <vklive/metal/metal_imgui_texture.h>

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

void imgui_init(MetalContext& ctx, const std::string& iniPath, bool viewports)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.IniFilename = iniPath.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (viewports)
    {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    if (!ImGui_ImplSDL2_InitForMetal(ctx.window))
    {
        throw std::runtime_error("Could not initialize ImGui SDL2 platform backend for Metal");
    }

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!ImGui_ImplMetal_Init(device))
    {
        throw std::runtime_error("Could not initialize ImGui Metal renderer backend");
    }
    if (!ImGui_ImplMetal_CreateDeviceObjects(device))
    {
        throw std::runtime_error("Could not create ImGui Metal renderer device objects");
    }

    ctx.spFontContext = std::make_shared<Zest::FontContext>();
    ctx.spFontTexture = std::make_shared<MetalImGuiTexture>(ctx);
    fonts_init(*ctx.spFontContext, ctx.spFontTexture.get());

    auto fontPath = Zest::runtree_find_path("fonts/Roboto-Regular.ttf");
    ctx.defaultFont = fonts_create(*ctx.spFontContext, "sans", fontPath.string().c_str());
}

void imgui_shutdown(MetalContext& ctx)
{
    if (ctx.spFontContext)
    {
        fonts_destroy(*ctx.spFontContext);
        ctx.spFontContext.reset();
    }
    ctx.spFontTexture.reset();

    ImGui_ImplMetal_Shutdown();
}

void imgui_render(MetalContext& ctx, ImDrawData* drawData)
{
    if (!context_begin_frame(ctx))
    {
        return;
    }

    auto drawable = bridge<id<CAMetalDrawable>>(ctx.currentDrawable);
    auto commandBuffer = bridge<id<MTLCommandBuffer>>(ctx.frameCommandBuffer);
    if (!drawable || !commandBuffer)
    {
        return;
    }

    MTLRenderPassDescriptor* passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    passDescriptor.colorAttachments[0].texture = drawable.texture;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.10, 0.10, 0.12, 1.00);

    ImGui_ImplMetal_NewFrame(passDescriptor);

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    ImGui_ImplMetal_RenderDrawData(drawData, commandBuffer, encoder);
    [encoder endEncoding];
}

} // namespace metal
