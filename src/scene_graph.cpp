#include <vklive/IDevice.h>
#include <vklive/scene.h>

extern IDevice* GetDevice();

void scenegraph_build(SceneGraph& scene)
{
    scene.sortedPasses.clear();

    // Naive for now
    for (auto& spPass : scene.passOrder)
    {
        scene.sortedPasses.push_back(spPass);
    }
}

IDeviceSurface* scenegraph_render(SceneGraph& scene, const glm::vec2& frameBufferSize)
{
    auto pDevice = GetDevice();

    //pDevice->WaitIdle();

    for (auto& [name, surface] : scene.surfaces)
    {
        pDevice->DestroySurface(*surface);
        surface->renderCount = 0;
    }
    // vulkan_scene_wait(ctx, pVulkanScene);

    // Make sure we have this surface
    auto updatePassSurface = [&](auto surfaceName) {
        auto& spSurface = scene.surfaces[surfaceName];
        auto size = spSurface->size;
        if (spSurface->path.empty())
        {
            if (size == glm::uvec2(0, 0))
            {
                size = glm::uvec2(spSurface->scale.x * frameBufferSize.x, frameBufferSize.y);
            }

            spSurface->currentSize = size;
        }
        return pDevice->AddOrUpdateSurface(*spSurface);
    };

    for (auto& pass : scene.sortedPasses)
    {
        std::vector<IDeviceSurface*> targets;
        IDeviceSurface* pDepthTarget = nullptr;

        // For this pass, ensure we have targets
        for (auto& target : pass->targets)
        {
            auto pTarget = updatePassSurface(target);
            if (format_is_depth(pTarget->pSurface->format))
            {
                pDepthTarget = pTarget;
            }
            else
            {
                targets.push_back(pTarget);
            }
        }

        // Samplers
        for (auto& sampler : pass->samplers)
        {
            updatePassSurface(sampler);
        }

        // IDeviceRenderPass* pRenderPass = pDevice->AddOrCreateRenderPass(targets, pDepthTarget);
        // IDeviceFrameBuffer* pFramebuffer = pDevice->AddOrCreateFrameBuffer(targets, pDepthTarget, renderPass);
        // IDevicePipeline
        // Draw
        /*// Walk the passes
        for (auto& [name, spPass] : scene.passes)
        {
            auto spVulkanPass = std::make_shared<VulkanPass>(spPass.get());

            // Renderpass for where this pass draws (the targets)
            vulkan_scene_create_renderpass(ctx, *spVulkanScene, *spVulkanPass);

            // Camera setup
            spPass->camera.nearFar = glm::vec2(0.1f, 256.0f);
            camera_set_pos_lookat(spPass->camera, glm::vec3(0.0f, 0.0f, 6.0f), glm::vec3(0.0f, 0.0f, 0.0f));

            // Uniform Buffer setup
            spVulkanPass->vsUniform = uniform_create(ctx, spVulkanPass->vsUBO);
            debug_set_buffer_name(ctx.device, spVulkanPass->vsUniform.buffer, debug_pass_name(*spVulkanPass, "Uniforms"));
            debug_set_devicememory_name(ctx.device, spVulkanPass->vsUniform.memory, debug_pass_name(*spVulkanPass, "UniformMemory"));

            spVulkanScene->passes[name] = spVulkanPass;
        }
        */
    }

    return pDevice->FindSurface("default_color");

}
