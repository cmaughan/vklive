#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include <vklive/metal/metal_surface.h>

struct Pass;

namespace metal
{

struct MetalContext;
struct MetalScene;

struct MetalPassTargets
{
    std::vector<MetalSurface*> colors;
    std::vector<uint32_t> colorFormats;
    MetalSurface* depth = nullptr;
    glm::uvec2 size = glm::uvec2(0);
    uint32_t depthFormat = 0;
};

struct MetalPass
{
    MetalPass(MetalScene& s, Pass& p)
        : metalScene(s)
        , pass(p)
    {
    }

    MetalScene& metalScene;
    Pass& pass;

    void* renderPipelineState = nullptr;
    void* depthStencilState = nullptr;
    void* uniformBuffer = nullptr;

    glm::uvec2 targetSize = glm::uvec2(0);
    std::vector<MetalSurfaceKey> colorTargetKeys;
    MetalSurfaceKey depthTargetKey;
    std::vector<uint64_t> colorTargetGenerations;
    uint64_t depthTargetGeneration = 0;
    std::vector<uint32_t> colorPixelFormats;
    uint32_t depthPixelFormat = 0;
    float lastUniformTime = 0.0f;

    bool reportedUnsupportedFeatures = false;
};

std::shared_ptr<MetalPass> metal_pass_create(MetalScene& scene, Pass& pass);
void metal_pass_destroy(MetalContext& ctx, MetalPass& pass);
bool metal_pass_draw(MetalContext& ctx, MetalPass& pass, const glm::uvec2& renderSize);

} // namespace metal
