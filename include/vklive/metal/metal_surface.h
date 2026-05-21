#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <zest/file/file.h>

struct Surface;

namespace metal
{

struct MetalContext;
struct MetalScene;

enum class MetalSurfaceFormat
{
    Unknown,
    RGBA8Unorm,
    RGBA8Unorm_sRGB,
    RGBA16Float,
    RGBA32Float,
    Depth32Float
};

enum class MetalAllocationState
{
    Init,
    Loaded,
    Failed
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
    MetalAllocationState allocationState = MetalAllocationState::Init;
    glm::uvec2 size = glm::uvec2(0);
    MetalSurfaceFormat format = MetalSurfaceFormat::Unknown;
    uint32_t mipLevels = 1;
    bool isAudioSurface = false;
    void* stagingBuffer = nullptr;
    uint64_t generation = 0;
};

std::shared_ptr<MetalSurface> metal_surface_create(MetalContext& ctx, MetalScene& scene, Surface& surface);
void metal_surface_destroy(MetalContext& ctx, MetalSurface& surface);
void metal_surface_create_target(MetalContext& ctx, MetalSurface& surface, const glm::uvec2& size, MetalSurfaceFormat format);
void metal_surface_create_sampler(MetalContext& ctx, MetalSurface& surface);
bool metal_surface_ensure_target(MetalContext& ctx, MetalScene& scene, MetalSurface& surface, const glm::uvec2& renderSize);
bool metal_surface_create_from_file(MetalContext& ctx, MetalSurface& surface, const fs::path& path, bool flipY = false);
bool metal_surface_create_from_memory(MetalContext& ctx, MetalSurface& surface, const fs::path& sourceName, const char* data, size_t dataSize, bool flipY = false);
bool metal_surface_update_from_audio(MetalContext& ctx, MetalSurface& surface);

} // namespace metal
