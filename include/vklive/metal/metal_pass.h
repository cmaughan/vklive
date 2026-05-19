#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include <vklive/metal/metal_surface.h>

struct Pass;

namespace metal
{

struct MetalContext;
struct MetalScene;

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
    MetalSurfaceKey colorTargetKey;
    MetalSurfaceKey depthTargetKey;
    uint64_t colorTargetGeneration = 0;
    uint64_t depthTargetGeneration = 0;
    uint32_t colorPixelFormat = 0;
    uint32_t depthPixelFormat = 0;
    float lastUniformTime = 0.0f;

    bool reportedUnsupportedFeatures = false;
};

std::shared_ptr<MetalPass> metal_pass_create(MetalScene& scene, Pass& pass);
void metal_pass_destroy(MetalContext& ctx, MetalPass& pass);
bool metal_pass_draw(MetalContext& ctx, MetalPass& pass, const glm::uvec2& renderSize);

} // namespace metal
