#pragma once

#include <vklive/IDevice.h>

#include <vklive/vulkan/vulkan_context.h>

struct SDL_Window;

namespace vulkan
{

// A thin wrapper around the vulkan code; enables the app to be independent of device
struct VulkanDevice : public IDevice
{
    VulkanDevice(SDL_Window* pWindow, const std::string& iniPath, bool viewports = false);
    ~VulkanDevice();
   
    // Interface
    virtual void InitScene(Scene& scene) override;
    virtual void DestroyScene(Scene& scene) override;

    virtual void WaitIdle() override;

    virtual void ImGui_Render(ImDrawData* pDrawData) override;
    virtual void ImGui_Render_3D(Scene& scene, bool backgroundRender) override;
    
    virtual void ValidateSwapChain() override;
    virtual void Present() override;

    virtual std::set<std::string> ShaderFileExtensions() override;

    DeviceContext& Context() override;

    VulkanContext ctx;
};

} // namespace vulkan
