#pragma once

#include <set>
#include <map>
#include <vector>
#include <mutex>
#include <functional>

#include <glm/glm.hpp>

struct SDL_Window;

struct ImDrawData;

struct IContextData
{
};

enum class DeviceState
{
    Normal,
    Lost,
    WasLost
};

struct SceneGraph;

struct IDeviceSurface
{
    IDeviceSurface(Surface* pS)
        : pSurface(pS)
    {
    }
    Surface* pSurface;
};

struct IDeviceRenderPass
{
    IDeviceRenderPass(Pass* pass)
        : pPass(pass)
    {
    }
    Pass* pPass;
};

struct DeviceContext
{
    SDL_Window* window = nullptr;
    bool minimized = false;

    std::shared_ptr<IContextData> spImGuiData;
    std::shared_ptr<IContextData> spRenderData;

    DeviceState deviceState = DeviceState::Normal;
    
    float hdpi = 1.0;
    float vdpi = 1.0;

    std::map<std::string, std::shared_ptr<IDeviceSurface>> mapDeviceSurfaces;
};

struct IDevice
{
    IDevice(){};
    virtual ~IDevice(){};
    IDevice& operator=(const IDevice&) = delete;
    IDevice(const IDevice&) = delete;
   
    // Device Methods
    virtual void InitScene(SceneGraph& scene) = 0;
    virtual void DestroyScene(SceneGraph& scene) = 0;
    virtual IDeviceSurface* FindSurface(const std::string& surface) const = 0;
    virtual IDeviceSurface* AddOrUpdateSurface(Surface& surface) = 0;
    virtual IDeviceRenderPass* AddOrUpdateRenderPass(SceneGraph& scene, Pass& pass, const std::vector<IDeviceSurface*>& targets, IDeviceSurface* pDepth = nullptr) = 0;
    virtual void DestroySurface(const Surface& surface) = 0;

    virtual void ImGui_Render(ImDrawData* pDrawData) = 0;
    virtual void ImGui_Render_3D(SceneGraph& scene, bool backgroundRender, const std::function<IDeviceSurface*(const glm::vec2&)>& fnDrawScene) = 0;
    virtual void WaitIdle() = 0;
    
    virtual void ValidateSwapChain() = 0;
    virtual void Present() = 0;

    virtual std::set<std::string> ShaderFileExtensions() = 0;

    virtual DeviceContext& Context() = 0;
};

