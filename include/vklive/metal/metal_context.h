#pragma once

#include <memory>
#include <unordered_map>

#include <vklive/IDevice.h>

struct Scene;

namespace metal
{

struct MetalImGuiTexture;
struct MetalScene;

struct MetalContext : DeviceContext
{
    void* metalView = nullptr;
    void* metalLayer = nullptr;
    void* device = nullptr;
    void* commandQueue = nullptr;
    void* currentDrawable = nullptr;
    void* frameCommandBuffer = nullptr;
    void* lastCommandBuffer = nullptr;

    std::shared_ptr<MetalImGuiTexture> spFontTexture;
    std::unordered_map<Scene*, std::shared_ptr<MetalScene>> mapMetalScene;
};

void context_init(MetalContext& ctx);
void context_destroy(MetalContext& ctx);
void context_validate_drawable_size(MetalContext& ctx);
bool context_begin_frame(MetalContext& ctx);
void context_present(MetalContext& ctx);
void context_wait_idle(MetalContext& ctx);

} // namespace metal
