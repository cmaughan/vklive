#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

struct Surface;

namespace metal
{

struct MetalContext;
struct MetalScene;

struct MetalSurfaceKey
{
    std::string targetName;
    uint64_t pingPongIndex = 0;

    bool operator==(const MetalSurfaceKey& other) const
    {
        return targetName == other.targetName && pingPongIndex == other.pingPongIndex;
    }

    explicit operator bool() const
    {
        return !targetName.empty();
    }

    struct HashFunction
    {
        size_t operator()(const MetalSurfaceKey& key) const
        {
            return std::hash<std::string>()(key.targetName) ^ key.pingPongIndex;
        }
    };
};

struct MetalSurface
{
    explicit MetalSurface(Surface* pS)
        : pSurface(pS)
    {
    }

    Surface* pSurface = nullptr;
    MetalSurfaceKey key;
    std::string debugName;
};

std::shared_ptr<MetalSurface> metal_surface_create(MetalContext& ctx, MetalScene& scene, Surface& surface);
void metal_surface_destroy(MetalContext& ctx, MetalSurface& surface);

} // namespace metal
