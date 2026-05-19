#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <vklive/IDevice.h>
#include <vklive/metal/metal_surface.h>

struct Pass;
struct Scene;

namespace metal
{

struct MetalContext;
struct MetalModel;
struct MetalPass;
struct MetalShader;
struct MetalSurface;

struct PathHash
{
    size_t operator()(const fs::path& path) const noexcept
    {
        return fs::hash_value(path);
    }
};

struct MetalScene
{
    explicit MetalScene(Scene* pS)
        : pScene(pS)
    {
    }

    Scene* pScene = nullptr;
    std::unordered_map<MetalSurfaceKey, std::shared_ptr<MetalSurface>, MetalSurfaceKey::HashFunction> surfaces;
    std::unordered_map<fs::path, std::shared_ptr<MetalModel>, PathHash> models;
    std::unordered_map<fs::path, std::shared_ptr<MetalShader>, PathHash> shaderStages;
    std::vector<std::shared_ptr<MetalPass>> passes;
    std::set<MetalSurfaceKey> viewableTargets;
    MetalSurfaceKey defaultTarget;

    bool reportedCaptureUnsupported = false;
};

std::shared_ptr<MetalScene> metal_scene_create(MetalContext& ctx, Scene& scene);
void metal_scene_destroy(MetalContext& ctx, Scene& scene);
std::shared_ptr<MetalScene> metal_scene_get(MetalContext& ctx, Scene& scene);

RenderOutput metal_scene_render(MetalContext& ctx, MetalScene& scene, const glm::vec2& size);
RenderOutput metal_scene_get_output(MetalContext& ctx, MetalScene& scene);
void metal_scene_write_to_file(MetalContext& ctx, MetalScene& scene, const fs::path& path);
std::vector<RenderTargetView> metal_scene_target_views(MetalContext& ctx, MetalScene& scene);
MetalSurface* metal_scene_get_or_create_surface(MetalContext& ctx, MetalScene& scene, const std::string& surfaceName, uint64_t frameCount = 0, bool sampling = false);
void metal_scene_prepare_output_targets(MetalContext& ctx, MetalScene& scene);

} // namespace metal
