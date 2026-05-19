#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <glm/glm.hpp>

struct Surface;

namespace metal
{

struct MetalContext;
struct MetalScene;

enum class MetalSurfaceFormat
{
    Unknown,
    RGBA8Unorm,
    RGBA16Float,
    RGBA32Float,
    Depth32Float
};

struct MetalSurfaceKey
{
    MetalSurfaceKey() = default;
    MetalSurfaceKey(const std::string& name, uint64_t frameCount, bool sampling = false);

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

    std::string DebugName() const;

    bool operator<(const MetalSurfaceKey& rhs) const;

    struct HashFunction
    {
        size_t operator()(const MetalSurfaceKey& key) const
        {
            size_t seed = std::hash<std::string>()(key.targetName);
            seed ^= std::hash<uint64_t>()(key.pingPongIndex) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
            return seed;
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
    void* texture = nullptr;
    void* sampler = nullptr;
    glm::uvec2 size = glm::uvec2(0);
    MetalSurfaceFormat format = MetalSurfaceFormat::Unknown;
    uint64_t generation = 0;
};

std::shared_ptr<MetalSurface> metal_surface_create(MetalContext& ctx, MetalScene& scene, Surface& surface);
void metal_surface_destroy(MetalContext& ctx, MetalSurface& surface);
void metal_surface_create_target(MetalContext& ctx, MetalSurface& surface, const glm::uvec2& size, MetalSurfaceFormat format);
void metal_surface_create_sampler(MetalContext& ctx, MetalSurface& surface);
bool metal_surface_ensure_target(MetalContext& ctx, MetalScene& scene, MetalSurface& surface, const glm::uvec2& renderSize);

} // namespace metal
