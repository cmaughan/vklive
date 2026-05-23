#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <zest/imgui/imgui.h>

#include <vklive/render_backend.h>

struct SDL_Window;
struct ImDrawData;

namespace fs = std::filesystem;

namespace Zest
{
struct FontContext;
struct IFontTexture;
}
struct IContextData
{
};

enum class DeviceState
{
    Normal,
    Lost,
    WasLost
};

struct Scene;
struct Surface;

struct RenderOutput
{
    ImTextureID textureId = 0;
    Surface* pSurface = nullptr;
};

struct RenderTargetView
{
    std::string name;
    ImTextureID textureId = 0;
    glm::uvec2 size = glm::uvec2(0);
};

struct DeviceContext
{
    SDL_Window* window = nullptr;
    bool minimized = false;

    std::shared_ptr<IContextData> spImGuiData;
    std::shared_ptr<IContextData> spRenderData;

    DeviceState deviceState = DeviceState::Normal;

    std::shared_ptr<Zest::FontContext> spFontContext;
    int defaultFont = 0;

    float hdpi = 1.0;
    float vdpi = 1.0;
};

struct IDevice
{
    IDevice() {};
    virtual ~IDevice() {};
    IDevice& operator=(const IDevice&) = delete;
    IDevice(const IDevice&) = delete;

    // Device Methods
    virtual void InitScene(Scene& scene) = 0;
    virtual void DestroyScene(Scene& scene) = 0;
    virtual void ImGui_Render(ImDrawData* pDrawData) = 0;

    virtual RenderOutput Render_3D(Scene& scene, const glm::vec2& size) = 0;
    virtual void WriteToFile(Scene& scene, const fs::path& path) = 0;

    virtual void WaitIdle() = 0;

    virtual void ValidateSwapChain() = 0;
    virtual bool Present() = 0;

    virtual std::string GetDeviceString() const = 0;
    virtual std::set<std::string> ShaderFileExtensions() = 0;

    virtual RenderBackend Backend() const = 0;
    virtual std::vector<RenderTargetView> TargetViews(Scene& scene) = 0;

    virtual DeviceContext& Context() = 0;
    virtual Zest::IFontTexture* FontTexture() { return nullptr; }
};
