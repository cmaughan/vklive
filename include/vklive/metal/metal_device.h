#pragma once

#include <vklive/IDevice.h>
#include <vklive/metal/metal_context.h>

struct SDL_Window;

namespace metal
{

struct MetalDevice : public IDevice
{
    MetalDevice(SDL_Window* pWindow, const std::string& iniPath, bool viewports = false);
    ~MetalDevice();

    virtual void InitScene(Scene& scene) override;
    virtual void DestroyScene(Scene& scene) override;

    virtual void WaitIdle() override;

    virtual void ImGui_Render(ImDrawData* pDrawData) override;

    virtual RenderOutput Render_3D(Scene& scene, const glm::vec2& size) override;
    virtual void WriteToFile(Scene& scene, const fs::path& path) override;

    virtual void ValidateSwapChain() override;
    virtual void Present() override;

    virtual std::set<std::string> ShaderFileExtensions() override;

    virtual std::string GetDeviceString() const override;

    virtual RenderBackend Backend() const override;
    virtual std::vector<RenderTargetView> TargetViews(Scene& scene) override;

    DeviceContext& Context() override;

    MetalContext ctx;
};

std::shared_ptr<IDevice> create_metal_device(SDL_Window* pWindow, const std::string& iniPath, bool viewports);

} // namespace metal
