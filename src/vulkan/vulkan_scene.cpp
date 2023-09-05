#include <atomic>
#include <fmt/format.h>
#include <unordered_set>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <zest/logger/logger.h>
#include <zest/file/runtree.h>
#include <zest/time/timer.h>

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
#include <vklive/vulkan/vulkan_model_as.h>
#include <vklive/vulkan/vulkan_imgui.h>

#include <zing/audio/audio.h>

using namespace ranges;

namespace vulkan
{

uint32_t VulkanScene::GlobalGeneration = 0;

std::ostream& operator<<(std::ostream& os, const SurfaceKey& key)
{
    os << key.DebugName();
    return os;
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

    spVulkanScene->generation = VulkanScene::GlobalGeneration++;

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
    for (auto& spPass : scene.passes)
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
    LOG_SCOPE(DBG, "Scene Destroy: " << vulkanScene.pScene << " Generation: " << vulkanScene.generation);

    // Destroying a scene means we might be destroying something that is in flight.
    // Lets wait for everything to finish
    //ctx.device.waitIdle();

    vulkanScene.defaultTarget = SurfaceKey();

    // Pass
    for (auto& pVulkanPass : vulkanScene.passes)
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

    SurfaceKey key(surfaceName, (!format_is_depth(pSurface->format) && pSurface->isTarget) ? frameCount : 0, sampling);

    auto itr = vulkanScene.surfaces.find(key);
    if (itr != vulkanScene.surfaces.end())
    {
        return itr->second.get();
    }

    auto spSurface = std::make_shared<VulkanSurface>(pSurface);
    vulkanScene.surfaces[key] = spSurface;
    spSurface->key = key;
    spSurface->debugName = key.DebugName();

    LOG(DBG, "Create VulkanSurface: " << *spSurface);
    return spSurface.get();
}

void vulkan_scene_target_build_descriptor(VulkanContext& ctx, VulkanScene& vulkanScene, VulkanSurface& vulkanSurface)
{
    if (vulkanSurface.ImGuiDescriptorSetLayout)
    {
        return;
    }

    auto imgui = imgui_context(ctx);
    auto binding = vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, &imgui->fontSampler);

    auto layoutCreateInfo = vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), binding);

    vulkanSurface.ImGuiDescriptorSetLayout = descriptor_create_layout(ctx, descriptor_get_cache(ctx), layoutCreateInfo);

    debug_set_descriptorsetlayout_name(ctx.device, vulkanSurface.ImGuiDescriptorSetLayout, fmt::format("{}:{}", to_string(vulkanSurface), "DescriptorLayout(UI)"));

    LOG(DBG, fmt::format("Build DescriptorSetLayout for: {}, {}", to_string(vulkanSurface), (void*)(VkDescriptorSetLayout)vulkanSurface.ImGuiDescriptorSetLayout));
}

void vulkan_scene_target_set_descriptor(VulkanContext& ctx, VulkanScene& vulkanScene, VulkanSurface& vulkanSurface)
{
    LOG_SCOPE(DBG, "Scene GUI Target Descriptors:");
    if (!vulkanSurface.ImGuiDescriptorSetLayout || !vulkanSurface.sampler)
    {
        vulkanSurface.ImGuiDescriptorSet = nullptr;
        return;
    }

    bool success = descriptor_allocate(ctx, descriptor_get_cache(ctx), &vulkanSurface.ImGuiDescriptorSet, vulkanSurface.ImGuiDescriptorSetLayout);
    if (!success)
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not allocate descriptor"));
        return;
    };

    debug_set_descriptorset_name(ctx.device, vulkanSurface.ImGuiDescriptorSet, fmt::format("{}:{}:{}", to_string(vulkanSurface), ctx.mainWindowData.frameIndex, "DescriptorSet(ImGui)"));

    assert(vulkanSurface.sampler != 0);
    vk::DescriptorImageInfo desc_image;
    desc_image.sampler = vulkanSurface.sampler;
    desc_image.imageView = vulkanSurface.view;
    desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet write_desc;
    write_desc.dstSet = vulkanSurface.ImGuiDescriptorSet;
    write_desc.descriptorCount = 1;
    write_desc.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write_desc.setImageInfo(desc_image);
    ctx.device.updateDescriptorSets(write_desc, {});
    
    LOG(DBG, fmt::format("Set DescriptorSet for: {}, Layout:{}, Set:{}, Sampler:{}", to_string(vulkanSurface), (void*)(VkDescriptorSetLayout)vulkanSurface.ImGuiDescriptorSetLayout, (void*)(VkDescriptorSet)vulkanSurface.ImGuiDescriptorSet, (void*)desc_image.sampler));
}

void vulkan_scene_prepare_output_descriptors(VulkanContext& ctx, VulkanScene& vulkanScene)
{
    vulkanScene.viewableTargets.clear();

    // Create descriptors if necessary for rendered targets
    // This is a little complex.  We walk the surfaces that are in the scene, finding the valid
    // ones for the current frame, that have been written.  Then we allocate our descriptors
    for (auto& [initKey, pVulkanSurface] : vulkanScene.surfaces)
    {
        // Find the correct surface for this frame
        SurfaceKey key(pVulkanSurface->pSurface->name, Scene::GlobalFrameCount, false);
        auto itrTarget = vulkanScene.surfaces.find(key);
        if (itrTarget != vulkanScene.surfaces.end())
        {
            // If the surface has been rendered and has a sampler, it's a potential for display
            auto pTarget = itrTarget->second.get();
            if (!pTarget->pSurface->rendered)
            {
                continue;
            }

            if (!vulkan_format_is_depth(pTarget->format))
            {
                if (!pTarget->sampler)
                {
                    surface_set_sampling(ctx, *pVulkanSurface);
                    LOG(DBG, "Adding sampler to rendered target for UI: " << pVulkanSurface->debugName);
                }
            }

            if (pTarget->sampler)
            {
                vulkanScene.viewableTargets.insert(key);
            }
        }
    }

    for (auto& key : vulkanScene.viewableTargets)
    {
        auto itrTarget = vulkanScene.surfaces.find(key);

        if (itrTarget != vulkanScene.surfaces.end())
        {
            auto pTarget = itrTarget->second.get();

            if (pTarget->pSurface->name == "default_color")
            {
                LOG(DBG, "Default Target: " << *pTarget);
                vulkanScene.defaultTarget = key;
            }

            // Only build on demand, but always set
            vulkan_scene_target_build_descriptor(ctx, vulkanScene, *pTarget);

            // Descriptors renewed each frame
            vulkan_scene_target_set_descriptor(ctx, vulkanScene, *pTarget);
        }
    }
}

void vulkan_scene_render(VulkanContext& ctx, VulkanScene& vulkanScene)
{
    assert(vulkanScene.pScene->valid);

    LOG(DBG, "Vulkan Scene Render: " << vulkanScene.pScene << " Generation: " << vulkanScene.generation);

    Scene::GlobalElapsedSeconds = Zest::timer_get_elapsed_seconds(Zest::globalTimer);
    ctx.descriptorCacheIndex++;
    Scene::GlobalFrameCount++;

    try
    {
        // Descriptor
        descriptor_reset_pools(ctx, descriptor_get_cache(ctx));

        // Copy the actual vertices to the GPU, if necessary.
        // TODO: Just the pass vertices instead of all
        for (auto& [_, pVulkanGeom] : vulkanScene.models)
        {
            vulkan_model_stage(ctx, *pVulkanGeom);
            if (pVulkanGeom->geometry.buildAS)
            {
                vulkan_model_build_acceleration_structure(ctx, *pVulkanGeom);
            }
        }

        vulkanScene.defaultTarget = SurfaceKey();

        // Draw the passes
        for (auto& pVulkanPass : vulkanScene.passes)
        {
            if (!vulkan_pass_draw(ctx, *pVulkanPass))
            {
                // Scene not valid, might be deleted
                return;
            }
        }

        vulkan_scene_prepare_output_descriptors(ctx, vulkanScene);
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
