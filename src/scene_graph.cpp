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

    for (auto& pPass : scene.sortedPasses)
    {
        std::vector<IDeviceSurface*> targets;
        IDeviceSurface* pDepthTarget = nullptr;

        // For this pass, ensure we have targets
        for (auto& target : pPass->targets)
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
        for (auto& sampler : pPass->samplers)
        {
            updatePassSurface(sampler);
        }

        auto pRenderPass = pDevice->AddOrUpdateRenderPass(scene, *pPass, targets, pDepthTarget);
        auto pFramebuffer = pDevice->AddOrUpdateFrameBuffer(scene, pRenderPass);

        // Camera setup
        pPass->camera.nearFar = glm::vec2(0.1f, 256.0f);
        camera_set_pos_lookat(pPass->camera, glm::vec3(0.0f, 0.0f, 6.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        camera_set_film_size(pPass->camera, glm::ivec2(pRenderPass->targetSize));
        camera_pre_render(pPass->camera);

        // Copy the actual vertices to the GPU, if necessary.
        /* for (auto& name : pPass->geometries)
        {
            model_stage(ctx, pVulkanGeom->model);
        }
        */

        // IDevicePipeline
        // Draw
        /*// Walk the passes
        for (auto& [name, spPass] : scene.passes)
        {
            // Uniform Buffer setup
            spVulkanPass->vsUniform = uniform_create(ctx, spVulkanPass->vsUBO);
            debug_set_buffer_name(ctx.device, spVulkanPass->vsUniform.buffer, debug_pass_name(*spVulkanPass, "Uniforms"));
            debug_set_devicememory_name(ctx.device, spVulkanPass->vsUniform.memory, debug_pass_name(*spVulkanPass, "UniformMemory"));
        }
        */
    }

    return pDevice->FindSurface("default_color");

}

/*
void vulkan_scene_render(VulkanContext& ctx, RenderContext& renderContext, SceneGraph& scene)
{
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    if (!pVulkanScene)
    {
        return;
    }

    // Prepare stuff that might have changed
    vulkan_scene_prepare(ctx, renderContext, scene);

    if (scene.valid == false)
    {
        return;
    }

    vulkan_scene_wait(ctx, pVulkanScene);

    pVulkanScene->commandBuffer.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    for (auto& [name, pVulkanPass] : pVulkanScene->passes)
    {
        auto size = pVulkanPass->targetSize;

        glm::vec3 meshPos = glm::vec3(0.0f, 0.0f, 0.0f);
        auto elapsed = timer_get_elapsed_seconds(globalTimer);
        pVulkanPass->vsUBO.iTimeDelta = (pVulkanPass->vsUBO.iTime == 0.0f) ? 0.0f : elapsed - pVulkanPass->vsUBO.iTime;
        pVulkanPass->vsUBO.iTime = elapsed;
        pVulkanPass->vsUBO.iFrameRate = elapsed != 0.0 ? (1.0f / elapsed) : 0.0;
        pVulkanPass->vsUBO.iSampleRate = 22000.0f; // Audio
        pVulkanPass->vsUBO.iGlobalTime = elapsed;
        pVulkanPass->vsUBO.iResolution = glm::vec4(size.x, size.y, 1.0, 0.0);
        pVulkanPass->vsUBO.iMouse = glm::vec4(0.0f);

        // TODO: year, month, day, seconds since EPOCH
        pVulkanPass->vsUBO.iDate = glm::vec4(0.0f);

        // TODO: Based on inputs when we have textures
        for (uint32_t i = 0; i < 4; i++)
        {
            pVulkanPass->vsUBO.iChannelResolution[i] = pVulkanPass->vsUBO.iResolution;
            pVulkanPass->vsUBO.iChannelTime[i] = pVulkanPass->vsUBO.iTime;

            pVulkanPass->vsUBO.iChannel[i].resolution = pVulkanPass->vsUBO.iChannelResolution[i];
            pVulkanPass->vsUBO.iChannel[i].time = pVulkanPass->vsUBO.iChannelTime[i];
        }

        // TODO: Used for offset into the sound buffer for the current frame, I think
        pVulkanPass->vsUBO.ifFragCoordOffsetUniform = glm::vec4(0.0f);

        pVulkanPass->vsUBO.model = glm::mat4(1.0f);
        pVulkanPass->vsUBO.view = camera_get_lookat(pVulkanPass->pPass->camera);
        pVulkanPass->vsUBO.projection = camera_get_projection(pVulkanPass->pPass->camera);
        pVulkanPass->vsUBO.modelViewProjection = pVulkanPass->vsUBO.projection * pVulkanPass->vsUBO.view * pVulkanPass->vsUBO.model;
        pVulkanPass->vsUBO.eye = glm::vec4(pVulkanPass->pPass->camera.position, 0.0f);
        utils_copy_to_memory(ctx, pVulkanPass->vsUniform.memory, pVulkanPass->vsUBO);

        // Clear
        vk::ClearValue clearValues[2];
        clearValues[0].color = clear_color(pVulkanPass->pPass->clearColor);
        clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        // Draw geometry
        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = pVulkanPass->renderPass;
        //renderPassBeginInfo.framebuffer = pVulkanPass->frameBuffer.framebuffer;
        renderPassBeginInfo.renderArea.extent.width = size.x;
        renderPassBeginInfo.renderArea.extent.height = size.y;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        auto rect = glm::uvec2(size.x, size.y);
        auto pVulkanPassPtr = pVulkanPass.get();

        try
        {
            auto& cmd = pVulkanScene->commandBuffer;
            debug_begin_region(cmd, debug_pass_name(*pVulkanPassPtr, "Draw"), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            cmd.setViewport(0, viewport(glm::uvec2(rect.x, rect.y)));
            cmd.setScissor(0, rect2d(glm::uvec2(rect.x, rect.y)));
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pVulkanPassPtr->geometryPipelineLayout, 0, pVulkanPassPtr->descriptorSets, {});
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pVulkanPassPtr->geometryPipeline);

            for (auto& geom : pVulkanPassPtr->pPass->geometries)
            {
                auto itrGeom = pVulkanScene->geometries.find(geom);
                if (itrGeom != pVulkanScene->geometries.end())
                {
                    auto pVulkanGeom = itrGeom->second;
                    cmd.bindVertexBuffers(0, pVulkanGeom->model.vertices.buffer, { 0 });
                    cmd.bindIndexBuffer(pVulkanGeom->model.indices.buffer, 0, vk::IndexType::eUint32);
                    cmd.drawIndexed(pVulkanGeom->model.indexCount, 1, 0, 0, 0);
                }
            }
            cmd.endRenderPass();
            debug_end_region(cmd);

            for (auto& col : pVulkanPassPtr->colorImages)
            {
                col->rendered = true;
            }

            if (pVulkanPassPtr->pDepthImage)
            {
                pVulkanPassPtr->pDepthImage->rendered = true;
            }
        }
        catch (std::exception& ex)
        {
            validation_error(ex.what());

            vulkan_scene_destroy(ctx, scene);

            ctx.deviceState = DeviceState::Lost;
            return;
        }
    }

    pVulkanScene->commandBuffer.end();
    pVulkanScene->inFlight = true;
    ctx.queue.submit(vk::SubmitInfo{ 0, nullptr, nullptr, 1, &pVulkanScene->commandBuffer }, pVulkanScene->fence);
}
*/
