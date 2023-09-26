#pragma once

#include <set>
#include <map>
#include <vector>
#include <mutex>
#include <filesystem>

#include <glm/glm.hpp>

struct SDL_Window;
struct ImDrawData;

namespace fs = std::filesystem;

namespace Zest
{
struct FontContext;
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
    void* textureId = nullptr;
    Surface* pSurface = nullptr;
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
    IDevice(){};
    virtual ~IDevice(){};
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
    virtual void Present() = 0;

    virtual std::string GetDeviceString() const = 0;
    virtual std::set<std::string> ShaderFileExtensions() = 0;

    virtual DeviceContext& Context() = 0;
};

