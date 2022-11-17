#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <vklive/platform/platform.h>
#include <vklive/scene.h>
#include <vklive/vulkan/vulkan_descriptor.h>

struct Scene;

namespace vulkan
{

struct VulkanPass;
struct VulkanShader;
struct VulkanModel;
struct VulkanSurface;

inline uint64_t frame_to_pingpong(uint64_t frame)
{
    return frame % 2;
}

struct SurfaceKey
{
    SurfaceKey(const std::string& name, uint64_t globalFrameCount, bool sampling = false)
        : targetName(name)
        , pingPongIndex(frame_to_pingpong(globalFrameCount))
    {
        if (sampling)
        {
            pingPongIndex = 1 - pingPongIndex;
        }
    }

    std::string targetName; // As declared in the pass
    uint64_t pingPongIndex; // Which pingpong buffer we are

    bool operator==(const SurfaceKey& other) const
    {
        return (targetName == other.targetName) && (pingPongIndex == other.pingPongIndex);
    }
    
    struct HashFunction
    {
        size_t operator()(const SurfaceKey& k) const
        {
            return std::hash<std::string>()(k.targetName) ^ k.pingPongIndex;
        }
    };
};

struct VulkanScene
{
    VulkanScene(Scene* pS)
        : pScene(pS)
    {
    }

    // Mappings from names to real vulkan objects
    Scene* pScene = nullptr;
    std::unordered_map<std::string, std::shared_ptr<VulkanSurface>> textureCacheMaybe;
    std::unordered_map<SurfaceKey, std::shared_ptr<VulkanSurface>, SurfaceKey::HashFunction> surfaces;
    std::unordered_map<fs::path, std::shared_ptr<VulkanModel>> models;
    std::unordered_map<fs::path, std::shared_ptr<VulkanShader>> shaderStages;
    std::unordered_map<std::string, std::shared_ptr<VulkanPass>> passes;

    DescriptorCache descriptorCache;
};

std::shared_ptr<VulkanScene> vulkan_scene_create(VulkanContext& ctx, Scene& scene);
VulkanScene* vulkan_scene_get(VulkanContext& ctx, Scene& scene);

void vulkan_scene_destroy(VulkanContext& ctx, VulkanScene& scene);
void vulkan_scene_render(VulkanContext& ctx, VulkanScene& vulkanScene);
void vulkan_scene_prepare(VulkanContext& ctx, VulkanScene& vulkanScene);

VulkanSurface* vulkan_scene_get_or_create_surface(VulkanScene& scene, const std::string& surface, uint64_t frameCount = 0, bool sampling = false);

} // namespace vulkan
