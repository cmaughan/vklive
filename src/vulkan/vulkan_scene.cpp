#include <atomic>
#include <fmt/format.h>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <vklive/logger/logger.h>

#include <vklive/file/runtree.h>
#include <vklive/time/timer.h>
#include <vklive/validation.h>

#include <vklive/vulkan/vulkan_command.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_descriptor.h>
#include <vklive/vulkan/vulkan_pass.h>
#include <vklive/vulkan/vulkan_pipeline.h>
#include <vklive/vulkan/vulkan_reflect.h>
#include <vklive/vulkan/vulkan_scene.h>
#include <vklive/vulkan/vulkan_shader.h>
#include <vklive/vulkan/vulkan_uniform.h>
#include <vklive/vulkan/vulkan_utils.h>

#include <vklive/audio/audio.h>

using namespace ranges;

namespace vulkan
{

DescriptorCache& vulkan_descriptor_cache(VulkanContext& ctx, VulkanScene& vulkanScene)
{
    // Keep enough descriptor pools to ensure we aren't re-using ones in flight
    return vulkanScene.descriptorCache[globalFrameCount % (ctx.mainWindowData.imageCount + 1)];
}

// Find the vulkan scene from the scene
VulkanScene* vulkan_scene_get(VulkanContext& ctx, Scene& scene)
{
    if (!scene.valid)
    {
        return nullptr;
    }
    auto itr = ctx.mapVulkanScene.find(&scene);
    if (itr == ctx.mapVulkanScene.end())
    {
        return nullptr;
    }
    return itr->second.get();
}

// Initialize a scene outside of the render loop
// We create what we can here: shaders, models, etc.
// - Create Vulkan specific structures
// - Compile shaders
// - Load models from files (don't upload geometry yet)
std::shared_ptr<VulkanScene> vulkan_scene_create(VulkanContext& ctx, Scene& scene)
{
    {
        // Cleanup previous
        auto pVulkanScene = vulkan_scene_get(ctx, scene);
        if (pVulkanScene)
        {
            vulkan_scene_destroy(ctx, *pVulkanScene);
        }
    }

    // Already has errors, we can't build vulkan info from it.
    if (!scene.errors.empty() || !fs::exists(scene.root) || !scene.valid)
    {
        return nullptr;
    }

    // Start assuming valid state
    scene.valid = true;

    auto spVulkanScene = std::make_shared<VulkanScene>(&scene);
    ctx.mapVulkanScene[&scene] = spVulkanScene;

    // Load Models
    for (auto& [_, pGeom] : scene.models)
    {
        vulkan_model_create(ctx, *spVulkanScene, *pGeom);
    }

    // Load Shaders
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (auto& [_, pShader] : scene.shaders)
    {
        // Might fail to create
        vulkan_shader_create(ctx, *spVulkanScene, *pShader);
    }

    // Walk the passes
    for (auto& [name, spPass] : scene.passes)
    {
        vulkan_pass_create(*spVulkanScene, *spPass);
    }

    // Cleanup
    if (!scene.valid)
    {
        vulkan_scene_destroy(ctx, *spVulkanScene);
        return nullptr;
    }

    return spVulkanScene;
}

void vulkan_scene_destroy(VulkanContext& ctx, VulkanScene& vulkanScene)
{
    LOG(DBG, "Scene Destroy");

    // Destroying a scene means we might be destroying something that is in flight.
    // Lets wait for everything to finish
    ctx.device.waitIdle();

    // Descriptor
    vulkan_descriptor_cleanup(ctx, vulkan_descriptor_cache(ctx, vulkanScene)); 

    // Pass
    for (auto& [name, pVulkanPass] : vulkanScene.passes)
    {
        vulkan_pass_destroy(ctx, *pVulkanPass);
    }
    vulkanScene.passes.clear();

    // Surfaces
    for (auto& [name, pVulkanSurface] : vulkanScene.surfaces)
    {
        vulkan_surface_destroy(ctx, *pVulkanSurface);
    }
    vulkanScene.surfaces.clear();

    // Shaders
    for (auto& [name, pShader] : vulkanScene.shaderStages)
    {
        vulkan_shader_destroy(ctx, *pShader);
    }
    vulkanScene.shaderStages.clear();

    // Models
    for (auto& [name, pGeom] : vulkanScene.models)
    {
        vulkan_model_destroy(ctx, *pGeom);
    }
    vulkanScene.models.clear();

    vulkanScene.pScene->valid = false;

    // Last thing, might erase the scene
    ctx.mapVulkanScene.erase(vulkanScene.pScene);
}

VulkanSurface* vulkan_scene_get_or_create_surface(VulkanScene& vulkanScene, const std::string& surfaceName, uint64_t frameCount, bool sampling)
{
    // Find this surface in the scene
    auto pSurface = scene_get_surface(*vulkanScene.pScene, surfaceName.c_str());
    if (!pSurface)
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not find surface: {}", surfaceName));
        return nullptr;
    }

    SurfaceKey key(surfaceName, pSurface->isTarget ? frameCount : 0, sampling);

    auto itr = vulkanScene.surfaces.find(key);
    if (itr != vulkanScene.surfaces.end())
    {
        return itr->second.get();
    }

    auto spSurface = std::make_shared<VulkanSurface>(pSurface);
    vulkanScene.surfaces[key] = spSurface;

    spSurface->debugName = fmt::format("{}:{}", surfaceName, key.pingPongIndex);
    return spSurface.get();
}

void vulkan_scene_render(VulkanContext& ctx, VulkanScene& vulkanScene)
{
    assert(vulkanScene.pScene->valid);

    globalElapsedSeconds = timer_get_elapsed_seconds(globalTimer);
    try
    {
        // Copy the actual vertices to the GPU, if necessary.
        // TODO: Just the pass vertices instead of all
        for (auto& [name, pVulkanGeom] : vulkanScene.models)
        {
            vulkan_model_stage(ctx, *pVulkanGeom);
        }

        descriptor_reset_pools(ctx, vulkan_descriptor_cache(ctx, vulkanScene)); 

        for (auto& [name, pVulkanPass] : vulkanScene.passes)
        {
            if (!vulkan_pass_draw(ctx, *pVulkanPass))
            {
                // Scene not valid, might be deleted
                return;
            }

        }
    }
    catch (std::exception& ex)
    {
        validation_error(ex.what());

        vulkan_scene_destroy(ctx, vulkanScene);

        ctx.deviceState = DeviceState::Lost;
        return;
    }
}

} // namespace vulkan
