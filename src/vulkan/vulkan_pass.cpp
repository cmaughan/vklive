#include <fmt/format.h>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <zing/audio/audio.h>

#include <zest/logger/logger.h>
#include <zest/string/string_utils.h>
#include <zest/time/timer.h>

#include "vklive/validation.h"

#include "vklive/vulkan/vulkan_pass.h"
#include "vklive/vulkan/vulkan_pipeline.h"
#include "vklive/vulkan/vulkan_render.h"
#include "vklive/vulkan/vulkan_uniform.h"
#include "vklive/vulkan/vulkan_utils.h"

using namespace ranges;

namespace vulkan
{
uint32_t alignedSize(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

VulkanPassSwapFrameData& vulkan_pass_frame_data(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    return vulkanPass.passFrameData[ctx.mainWindowData.frameIndex];
}

VulkanPassTargets& vulkan_pass_targets(VulkanContext& ctx, VulkanPassSwapFrameData& passFrameData)
{
    return passFrameData.passTargets[Scene::GlobalFrameCount % 2];
}

std::shared_ptr<VulkanPass> vulkan_pass_create(VulkanScene& vulkanScene, Pass& pass)
{
    auto spVulkanPass = std::make_shared<VulkanPass>(vulkanScene, pass);
    vulkanScene.passes.push_back(spVulkanPass);

    return spVulkanPass;
}

void vulkan_pass_destroy(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    for (auto& [index, passData] : vulkanPass.passFrameData)
    {
        LOG_SCOPE(DBG, "Pass Destroy: " << passData.debugName);

        // Wait for this pass to finish
        vulkan_pass_wait(ctx, passData);

        // Destroy pass specific fence and command pool
        ctx.device.destroyFence(passData.fence);
        ctx.device.destroyCommandPool(passData.commandPool);
        passData.commandPool = nullptr;

        // Pass buffers
        for (auto& [id, target] : passData.passTargets)
        {
            framebuffer_destroy(ctx, target.frameBuffer);
            target.frameBuffer = nullptr;
            ctx.device.destroyRenderPass(target.renderPass);
            target.renderPass = nullptr;
        }
        vulkan_buffer_destroy(ctx, passData.vsUniform);

        vulkan_buffer_destroy(ctx, passData.rayGenBindingTable);
        vulkan_buffer_destroy(ctx, passData.missBindingTable);
        vulkan_buffer_destroy(ctx, passData.hitBindingTable);

        // Pipeline/graphics
        // LOG(DBG, "Destroy GeometryPipe: " << passData.pipeline);
        ctx.device.destroyPipeline(passData.pipeline);
        ctx.device.destroyPipelineLayout(passData.geometryPipelineLayout);
        passData.pipeline = nullptr;
        passData.geometryPipelineLayout = nullptr;
    }
}

// Wait for this pass to finish sending to the hardware
void vulkan_pass_wait(VulkanContext& ctx, VulkanPassSwapFrameData& passData)
{
    LOG_SCOPE(DBG, "Wait for pass: " << passData.debugName);
    if (passData.inFlight)
    {
        const uint64_t FenceTimeout = 100000000;
        while (vk::Result::eTimeout == ctx.device.waitForFences(passData.fence, VK_TRUE, FenceTimeout))
            ;
        // We can now re-use the command buffer
        passData.commandBuffer.reset();
        passData.inFlight = false;
        ctx.device.resetFences(passData.fence);

        // LOG(DBG, "Reset fence: " << &passData.fence);
    }
    else
    {
        // LOG(DBG, "Not in flight: " << &passData.fence);
    }
}

void vulkan_pass_wait_all(VulkanContext& ctx, VulkanScene& scene)
{
    for (auto& vulkanPass : scene.passes)
    {
        for (auto& [flop, passData] : vulkanPass->passFrameData)
        {
            vulkan_pass_wait(ctx, passData);
        }
    }
}

// Allocate command buffer for this frame data
void vulkan_pass_prepare_command_buffers(VulkanContext& ctx, VulkanPassSwapFrameData& bufferData)
{
    // One time initialization
    if (!bufferData.commandBuffer)
    {
        // Fence, CommandPool, CommandBuffer
        bufferData.fence = ctx.device.createFence(vk::FenceCreateInfo());
        bufferData.commandPool = ctx.device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, ctx.graphicsQueue));
        bufferData.commandBuffer = ctx.device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(bufferData.commandPool, vk::CommandBufferLevel::ePrimary, 1))[0];

        debug_set_commandpool_name(ctx.device, bufferData.commandPool,
            "CommandPool:" + bufferData.debugName);
        debug_set_commandbuffer_name(ctx.device, bufferData.commandBuffer,
            "CommandBuffer:" + bufferData.debugName);
        debug_set_fence_name(ctx.device, bufferData.fence,
            "Fence:" + bufferData.debugName);
    }

    bufferData.commandBuffer.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
}

// Get a vulkan target surface.
// Will recreate if things have changed
VulkanSurface* get_vulkan_surface(VulkanContext& ctx, VulkanPass& vulkanPass, const std::string& surfaceName, bool sampling = false)
{
    VulkanScene& vulkanScene = vulkanPass.vulkanScene;

    // Get or create the vulkan specific equivalent
    auto pVulkanSurface = vulkan_scene_get_or_create_surface(vulkanScene, surfaceName, Scene::GlobalFrameCount, sampling);
    if (!pVulkanSurface)
    {
        return nullptr;
    }

    auto pSurface = pVulkanSurface->pSurface;
    if (!pSurface->isTarget)
    {
        if (pVulkanSurface->allocationState == VulkanAllocationState::Init)
        {
            if (!pSurface->path.empty())
            {
                auto file = scene_find_asset(*vulkanScene.pScene, pSurface->path, AssetType::Texture);
                if (file.empty() || !surface_create_from_file(ctx, *pVulkanSurface, file))
                {
                    scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not find surface: {}", pSurface->path.string()), vulkanScene.pScene->sceneGraphPath, vulkanPass.pass.scriptSamplersLine);
                    pVulkanSurface->allocationState = VulkanAllocationState::Failed;
                    return nullptr;
                }
            }
        }

        // TODO: Cleanup this approach.  Think about it some more
        if (pSurface->name == "AudioAnalysis")
        {
            // Only update the audio surface once
            if (vulkanScene.audioSurfaceFrameGeneration != Scene::GlobalFrameCount || (pVulkanSurface->allocationState == VulkanAllocationState::Init))
            {
                vulkanScene.audioSurfaceFrameGeneration = Scene::GlobalFrameCount;

                bool surfaceChanged = false;
                surface_update_from_audio(ctx, *pVulkanSurface, surfaceChanged, vulkan_pass_frame_data(ctx, vulkanPass).commandBuffer);
            }
        }
        return pVulkanSurface;
    }

    // If it is 0 size, then it is frame buffer size
    auto size = pSurface->size;
    if (size == glm::uvec2(0, 0))
    {
        // Size is the frame buffer size, by any multiplier the user has provided
        size = glm::uvec2(pSurface->scale.x * ctx.frameBufferSize.x,
            pSurface->scale.y * ctx.frameBufferSize.y);
    }

    // OK, so it has changed size
    if (size != pVulkanSurface->currentSize)
    {
        LOG(DBG, "Resize: " << *pVulkanSurface);

        // Wait for this pass to complete, since we are destroying potentially active surfaces
        // NOTE: We wait idle because, the sampler is begin used in the IMGui pass, so we can't just
        // wait for the pass, we would need to wait for ImGui.
        // vulkan_pass_wait_all(ctx, vulkanScene);
        ctx.device.waitIdle();

        vulkan_surface_destroy(ctx, *pVulkanSurface);

        // Update to latest, even if we fail, so we don't keep trying
        pVulkanSurface->currentSize = size;

        // If surface bigger than 0
        if (size != glm::uvec2(0, 0))
        {
            if (format_is_depth(pSurface->format))
            {
                vulkan_surface_create_depth(ctx, *pVulkanSurface, size, utils_format_to_vulkan(pSurface->format), true);
            }
            else
            {
                vulkan_surface_create(ctx, *pVulkanSurface, size, utils_format_to_vulkan(pSurface->format), true);
            }
        }

        pVulkanSurface->pSurface->rendered = false;
    }
    return pVulkanSurface;
}

VulkanSurface* get_vulkan_surface(VulkanContext& ctx, VulkanPass& vulkanPass, PassSampler& passSampler)
{
    return get_vulkan_surface(ctx, vulkanPass, passSampler.sampler, passSampler.sampleAlternate);
}

// Ensure that allocated targets match
bool vulkan_pass_check_targets(VulkanContext& ctx, VulkanPassTargets& passTargets)
{
    auto checkSize = [&](auto img, auto& size) {
        // Ignore surfaces that aren't created
        if (!img)
        {
            return true;
        }

        if (size.x == 0 && size.y == 0)
        {
            size = glm::uvec2(img->extent.width, img->extent.height);
        }
        else
        {
            if (size.x != img->extent.width || size.y != img->extent.height)
            {
                auto& pass = passTargets.pFrameData->pVulkanPass->pass;
                Message msg;
                msg.text = "Target sizes don't match";
                msg.path = pass.scene.sceneGraphPath;
                msg.line = pass.scriptTargetsLine;
                msg.severity = MessageSeverity::Error;

                // Any error invalidates the scene
                pass.scene.errors.push_back(msg);

                return false;
            }
        }
        return true;
    };

    auto checkForChanges = [&](auto pVulkanSurface) {
        if (!pVulkanSurface)
        {
            return false;
        }

        bool changed = false;
        auto itr = passTargets.mapSurfaceGenerations.find(pVulkanSurface);
        if (itr != passTargets.mapSurfaceGenerations.end())
        {
            if (itr->second != pVulkanSurface->generation)
            {
                changed = true;
            }
        }
        else
        {
            changed = true;
        }
        passTargets.mapSurfaceGenerations[pVulkanSurface] = pVulkanSurface->generation;
        return changed;
    };

    bool diff = false;
    glm::uvec2 size = glm::uvec2(0);

    for (auto& pTargetData : passTargets.orderedTargets)
    {
        if (!checkSize(pTargetData->pVulkanSurface, size))
        {
            // If sizes don't match, we are effectively broken and can't render
            return false;
        }

        if (checkForChanges(pTargetData->pVulkanSurface))
        {
            diff = true;
        }
    }

    /* if (!checkSize(passTargets.depth, size))
    {
        return false;
    }

    if (checkForChanges(passTargets.depth))
    {
        diff = true;
    }
    */

    // If our pass targets have changed size, then we need to clean up the framebuffer, renderpass and geom pipe
    // TODO: This only handles resizes, not recreation?...
    if (diff)
    {
        // LOG(DBG, "Targets different, cleaning up FB, RenderPass, Geom");

        // I think we can just wait for the pass here, since these are all pass-specific objects
        // Note that a previous pass may have changed targets, even if this one didn't.  So we need
        // to update our state here
        vulkan_pass_wait(ctx, *passTargets.pFrameData);

        // destroy the framebuffer since it depends on renderpass and targets
        if (passTargets.frameBuffer)
        {
            // LOG(DBG, "Destroy Framebuffer: " << passTargets.frameBuffer);
            framebuffer_destroy(ctx, passTargets.frameBuffer);
            passTargets.frameBuffer = nullptr;
        }

        // Renderpass has the framebuffer, which we had to recreate, so recreate render pass
        if (passTargets.renderPass)
        {
            // LOG(DBG, "Destroy RenderPass: " << passTargets.renderPass);
            ctx.device.destroyRenderPass(passTargets.renderPass);
            passTargets.renderPass = nullptr;
        }

        // Geom pipe uses the render pass
        if (passTargets.pFrameData && passTargets.pFrameData->pipeline)
        {
            // LOG(DBG, "Destroy Geometry Pipe: " << passTargets.pFrameData->pipeline);
            ctx.device.destroyPipeline(passTargets.pFrameData->pipeline);
            ctx.device.destroyPipelineLayout(passTargets.pFrameData->geometryPipelineLayout);
            passTargets.pFrameData->pipeline = nullptr;
            passTargets.pFrameData->geometryPipelineLayout = nullptr;
        }
    }

    passTargets.targetSize = size;

    return true;
}

void vulkan_pass_check_samplers(VulkanContext& ctx, VulkanPassTargets& passTargets, VulkanPassSwapFrameData& frameData)
{
    auto checkForChanges = [&](auto pVulkanSurface) {
        if (!pVulkanSurface)
        {
            return false;
        }

        bool changed = false;
        auto itr = passTargets.mapSurfaceGenerations.find(pVulkanSurface);
        if (itr != passTargets.mapSurfaceGenerations.end())
        {
            if (itr->second != pVulkanSurface->generation)
            {
                changed = true;
            }
        }
        else
        {
            changed = true;
        }
        passTargets.mapSurfaceGenerations[pVulkanSurface] = pVulkanSurface->generation;
        return changed;
    };

    for (auto& passSampler : frameData.pVulkanPass->pass.samplers)
    {
        auto pVulkanSurface = get_vulkan_surface(ctx, *frameData.pVulkanPass, passSampler);
        if (checkForChanges(pVulkanSurface))
        {
            // LOG(DBG, "Sampler has changed, removing geometry pipe");

            // I think we can just wait for the pass here, since these are all pass-specific objects
            // Note that a previous pass may have changed targets, even if this one didn't.  So we need
            // to update our state here
            vulkan_pass_wait(ctx, *passTargets.pFrameData);

            // Geom pipe uses the render pass
            if (frameData.pipeline)
            {
                // LOG(DBG, "Destroy Geometry Pipe: " << frameData.pipeline);
                ctx.device.destroyPipeline(frameData.pipeline);
                ctx.device.destroyPipelineLayout(frameData.geometryPipelineLayout);
                passTargets.pFrameData->pipeline = nullptr;
                passTargets.pFrameData->geometryPipelineLayout = nullptr;
            }

            // We are sampling this surface, so make sure it has a sampler:
            // they are not automatically created until the surface is actually sampled
            if (!pVulkanSurface->sampler)
            {
                surface_create_sampler(ctx, *pVulkanSurface);
                debug_set_sampler_name(ctx.device, pVulkanSurface->sampler, pVulkanSurface->debugName);
            }
        }
    }
}

void vulkan_pass_prepare_renderpass(VulkanContext& ctx, VulkanPassTargets& passTargets)
{
    if (!passTargets.renderPass)
    {
        std::vector<vk::Format> colorFormats;
        vk::Format depthFormat = vk::Format::eUndefined;

        for (auto& pTargetData : passTargets.orderedTargets)
        {
            if (vulkan_format_is_depth(pTargetData->pVulkanSurface->format))
            {
                depthFormat = pTargetData->pVulkanSurface->format;
            }
            else
            {
                colorFormats.push_back(pTargetData->pVulkanSurface->format);
            }
        }

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;

        // Color
        std::vector<vk::AttachmentDescription> attachments;
        std::vector<vk::AttachmentReference> colorAttachmentReferences;
        attachments.resize(colorFormats.size());
        colorAttachmentReferences.resize(attachments.size());

        // We can read the shader in the pixel shader
        vk::ImageUsageFlags attachmentUsage{ vk::ImageUsageFlagBits::eSampled };
        vk::ImageLayout colorFinalLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };
        for (uint32_t i = 0; i < attachments.size(); ++i)
        {
            attachments[i].format = colorFormats[i];
            attachments[i].loadOp = passTargets.pFrameData->pVulkanPass->pass.hasClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
            attachments[i].storeOp = colorFinalLayout == vk::ImageLayout::eColorAttachmentOptimal ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore;
            attachments[i].initialLayout = vk::ImageLayout::eUndefined;
            attachments[i].finalLayout = colorFinalLayout; // We want a color buffer to read

            vk::AttachmentReference& attachmentReference = colorAttachmentReferences[i];
            attachmentReference.attachment = i;
            attachmentReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

            subpass.colorAttachmentCount = (uint32_t)colorAttachmentReferences.size();
            subpass.pColorAttachments = colorAttachmentReferences.data();
        }

        // Depth
        vk::AttachmentReference depthAttachmentReference;
        vk::ImageLayout depthFinalLayout{ vk::ImageLayout::eDepthStencilAttachmentOptimal };
        if (depthFormat != vk::Format::eUndefined)
        {
            vk::AttachmentDescription depthAttachment;
            depthAttachment.format = depthFormat;
            depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
            // We might be using the depth attacment for something, so preserve it if it's final layout is not undefined
            depthAttachment.storeOp = depthFinalLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore;
            depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
            depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
            depthAttachment.finalLayout = depthFinalLayout;
            attachments.push_back(depthAttachment);
            depthAttachmentReference.attachment = (uint32_t)attachments.size() - 1;
            depthAttachmentReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            subpass.pDepthStencilAttachment = &depthAttachmentReference;
        }

        std::vector<vk::SubpassDependency> subpassDependencies;
        if ((colorFinalLayout != vk::ImageLayout::eColorAttachmentOptimal) && (colorFinalLayout != vk::ImageLayout::eUndefined))
        {
            // Implicit transition
            vk::SubpassDependency dependency;
            dependency.srcSubpass = 0;
            dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

            dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstAccessMask = utils_access_flags_for_layout(colorFinalLayout);
            dependency.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
            subpassDependencies.push_back(dependency);
        }

        if ((depthFinalLayout != vk::ImageLayout::eDepthStencilAttachmentOptimal) && (depthFinalLayout != vk::ImageLayout::eUndefined))
        {
            // Implicit transition
            // Write color...
            vk::SubpassDependency dependency;
            dependency.srcSubpass = 0;
            dependency.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            dependency.srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;

            // Read color after end render pass.
            dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstAccessMask = utils_access_flags_for_layout(depthFinalLayout);
            dependency.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
            subpassDependencies.push_back(dependency);
        }

        // TODO:
        // Performance improvement/fix the dependencies here based on the scene graph.
        // We don't always need to read, for example.
        // This code is almost certainly setting up the dependencies here badly; I just haven't looked into how to do it properly based on
        // the targets and samplers of each pass...
        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = (uint32_t)attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = (uint32_t)subpassDependencies.size();
        renderPassInfo.pDependencies = subpassDependencies.data();
        passTargets.renderPass = ctx.device.createRenderPass(renderPassInfo);
        debug_set_renderpass_name(ctx.device, passTargets.renderPass, fmt::format("RenderPass:{}", passTargets.debugName));
    }

    if (!passTargets.frameBuffer)
    {
        vulkan_framebuffer_create(ctx, passTargets.frameBuffer, passTargets, passTargets.renderPass);

        debug_set_framebuffer_name(ctx.device, passTargets.frameBuffer, "FrameBuffer:" + passTargets.debugName);
    }

    // LOG(DBG, "  FrameBuffer: " << passTargets.frameBuffer);
    // LOG(DBG, "  RenderPass: " << passTargets.renderPass);
}

void vulkan_pass_dump_targets(VulkanPassTargets& passTargets)
{
    LOG_SCOPE(DBG, "Targets:");
    for (auto& pTargetData : passTargets.orderedTargets)
    {
        LOG(DBG, "Target: " << *pTargetData->pVulkanSurface);
    }

    /*
    if (passTargets.depth)
    {
        LOG(DBG, "  Name: " << passTargets.depth->debugName << " Target: " << passTargets.depth->debugName);
    }
    */
}

void vulkan_pass_dump_samplers(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    if (vulkanPass.pass.samplers.empty())
    {
        return;
    }
    LOG_SCOPE(DBG, "Samplers:");
    for (auto& passSampler : vulkanPass.pass.samplers)
    {
        auto pVulkanSurface = get_vulkan_surface(ctx, vulkanPass, passSampler);
        if (pVulkanSurface)
        {
            LOG_SCOPE(DBG, "Name: " << pVulkanSurface->debugName);
            LOG(DBG, "Sample Alternate: " << passSampler.sampleAlternate);
            LOG(DBG, "Loaded: " << (pVulkanSurface->allocationState == VulkanAllocationState::Loaded));
            LOG(DBG, "Image: " << pVulkanSurface->image);
            LOG(DBG, "Extent: " << pVulkanSurface->extent.width << ", " << pVulkanSurface->extent.height << ", " << pVulkanSurface->extent.depth);
            LOG(DBG, "MipLevels: " << pVulkanSurface->mipLevels);
            LOG(DBG, "Format: " << to_string(pVulkanSurface->format));
        }
        else
        {
            LOG(DBG, "NOT Found: " << passSampler.sampler);
        }
    }
}

void vulkan_pass_prepare_targets(VulkanContext& ctx, VulkanPassSwapFrameData& passFrameData)
{
    bool targetsChanged = false;

    // Flip-flop between 2 pages for this swap index; so we can read the target later
    auto& pass = passFrameData.pVulkanPass->pass;
    auto& vulkanScene = passFrameData.pVulkanPass->vulkanScene;
    auto& vulkanPassTargets = passFrameData.passTargets[Scene::GlobalFrameCount % 2];

    vulkanPassTargets.debugName = fmt::format("PassTargets:{}:{}", Scene::GlobalFrameCount % 2, passFrameData.debugName);

    LOG_SCOPE(DBG, "PrepareTargets: " << vulkanPassTargets.debugName);

    vulkanPassTargets.orderedTargets.clear();

    // Make sure all targets are relevent
    for (auto& surfaceName : pass.targets)
    {
        auto& targetData = vulkanPassTargets.mapNameToTargetData[surfaceName];
        targetData.pVulkanSurface = get_vulkan_surface(ctx, *passFrameData.pVulkanPass, surfaceName);

        if (targetData.pVulkanSurface->pSurface->isRayTarget)
        {
            surface_set_layout(ctx, passFrameData.commandBuffer, targetData.pVulkanSurface->image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
        }
        vulkanPassTargets.orderedTargets.push_back(&targetData);
    }

    if (!vulkan_pass_check_targets(ctx, vulkanPassTargets))
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, "Targets not the same size", vulkanScene.pScene->sceneGraphPath);
    }

    vulkan_pass_dump_targets(vulkanPassTargets);
}

// Surfaces that aren't targets
void vulkan_pass_prepare_surfaces(VulkanContext& ctx, VulkanPassSwapFrameData& passFrameData)
{
    auto& pass = passFrameData.pVulkanPass->pass;
    auto& vulkanPassTargets = passFrameData.passTargets[Scene::GlobalFrameCount % 2];

    LOG_SCOPE(DBG, "Prepare Surfaces:");

    // Walk the surfaces
    for (auto& passSampler : pass.samplers)
    {
        auto pVulkanSurface = get_vulkan_surface(ctx, *passFrameData.pVulkanPass, passSampler);
        if (!pVulkanSurface)
        {
            scene_report_error(pass.scene, MessageSeverity::Error, fmt::format("Surface not found: ", passSampler.sampler), pass.scene.sceneGraphPath);
        }

        vulkan_pass_check_samplers(ctx, vulkanPassTargets, passFrameData);
    }

    vulkan_pass_dump_samplers(ctx, *passFrameData.pVulkanPass);
}

// Ensure we have setup the buffers for this pass
void vulkan_pass_prepare_uniforms(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    PROFILE_SCOPE(prepare_uniforms);
    LOG_SCOPE(DBG, "Prepare Uniforms:");

    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(ctx, passFrameData);

    if (!passFrameData.vsUniform.buffer)
    {
        passFrameData.vsUniform = vulkan_uniform_create(ctx, passFrameData.vsUBO);
        debug_set_buffer_name(ctx.device,
            passFrameData.vsUniform.buffer,
            fmt::format("{}:{}",
                passFrameData.debugName, "Uniforms"));

        debug_set_devicememory_name(ctx.device,
            passFrameData.vsUniform.memory,
            fmt::format("{}:{}", passFrameData.debugName, "DeviceMemory"));
    }

    auto size = passTargets.targetSize;
    auto& ubo = passFrameData.vsUBO;

    auto& scene = *vulkanPass.vulkanScene.pScene;

    // Setup the camera for this pass
    // pVulkanPass->pPass->camera.orbitDelta = glm::vec2(4.0f, 0.0f);
    // Note that the camera might need different setup each time
    for (auto& cameraName : vulkanPass.pass.cameras)
    {
        auto itrCamera = scene.cameras.find(cameraName);
        if (itrCamera != scene.cameras.end())
        {
            auto& camera = *itrCamera->second;
            camera_set_film_size(camera, glm::ivec2(size));
            camera_pre_render(camera);

            // Set UBO variables
            // TODO: More than one camera; handle with reflection
            ubo.view = camera_get_lookat(camera);
            ubo.projection = camera_get_projection(camera);
            ubo.modelViewProjection = ubo.projection * ubo.view * ubo.model;
            ubo.viewInverse = glm::inverse(ubo.view);
            ubo.projectionInverse = glm::inverse(ubo.projection);
            ubo.eye = glm::vec4(camera.position, 0.0f);
        }
    }

    auto elapsed = Scene::GlobalElapsedSeconds;
    glm::vec3 meshPos = glm::vec3(0.0f, 0.0f, 0.0f);
    ubo.iTimeDelta = (ubo.iTime == 0.0f) ? 0.0f : elapsed - ubo.iTime;
    ubo.iTime = elapsed;
    ubo.iFrame = Scene::GlobalFrameCount;
    ubo.iFrameRate = elapsed != 0.0 ? (1.0f / elapsed) : 0.0;
    ubo.iGlobalTime = elapsed;
    ubo.iResolution = glm::vec4(size.x, size.y, 1.0, 0.0);
    ubo.iMouse = glm::vec4(0.0f); // TODO: Mouse
    ubo.iSceneFlags = scene.sceneFlags;

    ubo.vertexSize = layout_size(g_vertexLayout);

    // Audio
    auto& audioCtx = Zing::GetAudioContext();
    ubo.iSampleRate = audioCtx.audioDeviceSettings.sampleRate;
    auto channels = std::clamp(audioCtx.analysisChannels.size(), (size_t)1, (size_t)4);
    // TBD: We can have more channels (2 in, 2 out, for example).
    // Need to let the shader author map what they want
    for (auto [Id, pAnalysis] : audioCtx.analysisChannels)
    {
        if (pAnalysis->thisChannel.second < 2)
        {
            // Lock free atomic
            ubo.iSpectrumBands[pAnalysis->thisChannel.second] = pAnalysis->spectrumBands.load();
        }
    }

    // TODO: year, month, day, seconds since EPOCH
    ubo.iDate = glm::vec4(0.0f);

    // TODO: Based on input sizes; shouldn't be same as target necessarily.
    for (uint32_t i = 0; i < 4; i++)
    {
        ubo.iChannelResolution[i] = ubo.iResolution;
        ubo.iChannelTime[i] = ubo.iTime;

        ubo.iChannel[i].resolution = ubo.iChannelResolution[i];
        ubo.iChannel[i].time = ubo.iChannelTime[i];
    }

    // TODO: Used for offset into the sound buffer for the current frame, I think
    ubo.ifFragCoordOffsetUniform = glm::vec4(0.0f);

    ubo.model = glm::mat4(1.0f);

    // TODO: Should we stage using command buffer?  This is a direct write
    utils_copy_to_memory(ctx, passFrameData.vsUniform.memory, ubo);
}

bool vulkan_pass_build_descriptors(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    PROFILE_SCOPE(build_descriptors);

    VulkanScene& vulkanScene = vulkanPass.vulkanScene;
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(ctx, passFrameData);

    if (passFrameData.builtDescriptors)
    {
        return true;
    }

    LOG_SCOPE(DBG, "Build Descriptors:");

    passFrameData.builtDescriptors = true;

    // Lookup the vulkan compiled shader stages that match the declared scene shaders
    auto& stages = vulkanPass.vulkanScene.shaderStages;

    auto f = [&](auto& p) { return stages.find(p) != stages.end(); };
    auto t = [&](auto& p) { return &stages.find(p)->second->bindingSets; };
    auto bindings = vulkanPass.pass.shaders | views::filter(f) | views::transform(t) | to<std::vector>();
    if (!bindings_merge(vulkanPass, bindings, passFrameData.mergedBindingSets))
    {
        return false;
    }

    LOG(DBG, "Pass: " << passFrameData.debugName << ", Merged Bindings:");
    bindings_dump(passFrameData.mergedBindingSets);

    passFrameData.descriptorSetBindings.clear();
    passFrameData.descriptorSetLayouts.clear();

    for (auto& [set, bindingSet] : passFrameData.mergedBindingSets)
    {
        LOG(DBG, "Set: " << set);

        auto& bindings = passFrameData.descriptorSetBindings[set];
        auto& layout = passFrameData.descriptorSetLayouts[set];

        bindings.clear();
        layout = nullptr;

        for (auto& [index, binding] : bindingSet.bindings)
        {
            auto itrMeta = bindingSet.bindingMeta.find(index);
            if (itrMeta == bindingSet.bindingMeta.end())
            {
                scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not find binding for index: {}", index), itrMeta->second.shaderPath, itrMeta->second.line, itrMeta->second.range);
                return false;
            }

            bindings.push_back(binding);
        }

        if (!bindings.empty())
        {
            // build layout first
            vk::DescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.pNext = nullptr;
            layoutInfo.pBindings = bindings.data();
            layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());

            auto descriptorSetLayout = descriptor_create_layout(ctx, descriptor_get_cache(ctx), layoutInfo);
            debug_set_descriptorsetlayout_name(ctx.device, descriptorSetLayout, fmt::format("{}:{}", passFrameData.debugName, "Layout"));

            layout = descriptorSetLayout;
        }
    }
    return true;
}

void vulkan_pass_set_descriptors(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    PROFILE_SCOPE(set_descriptors);

    LOG_SCOPE(DBG, "Set Descriptors:");

    VulkanScene& vulkanScene = vulkanPass.vulkanScene;

    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(ctx, passFrameData);

    // Build pointers to image infos for later
    std::map<std::string, vk::DescriptorImageInfo> imageInfos;
    for (auto& passSampler : vulkanPass.pass.samplers)
    {
        // TODO: Correct sampler; SurfaceKey needs to account for target ping/pong
        auto pVulkanSurface = get_vulkan_surface(ctx, vulkanPass, passSampler);
        if (pVulkanSurface && pVulkanSurface->image && pVulkanSurface->view && pVulkanSurface->sampler)
        {
            vk::DescriptorImageInfo desc_image;
            assert(pVulkanSurface->sampler != 0);
            desc_image.sampler = pVulkanSurface->sampler;
            desc_image.imageView = pVulkanSurface->view;
            desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            imageInfos[passSampler.sampler] = desc_image;
        }
    }

    // When using a target in a descriptor, for ray tracing, we set the general layout
    if (vulkanPass.pass.passType == PassType::RayTracing)
    {
        for (auto& pTarget : passTargets.orderedTargets)
        {
            if (pTarget->pVulkanSurface->image)
            {
                vk::DescriptorImageInfo desc_image;
                desc_image.imageView = pTarget->pVulkanSurface->view;
                desc_image.imageLayout = vk::ImageLayout::eGeneral;
                imageInfos[pTarget->pVulkanSurface->pSurface->name] = desc_image;
            }
        }
    }

    passFrameData.descriptorSets.clear();

    std::vector<vk::WriteDescriptorSet> writes;

    for (auto& [set, bindings] : passFrameData.descriptorSetBindings)
    {
        auto& layout = passFrameData.descriptorSetLayouts[set];
        auto& bindingSet = passFrameData.mergedBindingSets[set];
        if (!layout)
        {
            continue;
        }

        if (bindings.empty())
        {
            continue;
        }

        // build layout first
        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.pNext = nullptr;
        layoutInfo.pBindings = bindings.data();
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());

        vk::DescriptorSet descriptorSet;
        bool success = descriptor_allocate(ctx, descriptor_get_cache(ctx), &descriptorSet, layout);
        if (!success)
        {
            scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not allocate descriptor"));
            return;
        };
        debug_set_descriptorset_name(ctx.device, descriptorSet, fmt::format("{}:{}", passFrameData.debugName, "DescriptorSet"));

        passFrameData.descriptorSets.push_back(descriptorSet);

        // Get bindings for this set
        for (auto& binding : bindings)
        {
            auto index = binding.binding;
            auto itrMeta = bindingSet.bindingMeta.find(index);
            if (itrMeta == bindingSet.bindingMeta.end())
            {
                scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not find binding for index: {}", index), itrMeta->second.shaderPath, itrMeta->second.line, itrMeta->second.range);
                return;
            }

            // TODO: Binding count.  When is there more than 1 specified? An array in the shader?
            vk::WriteDescriptorSet newWrite{};
            newWrite.pNext = nullptr;
            newWrite.descriptorCount = 1;
            newWrite.descriptorType = binding.descriptorType;
            newWrite.dstBinding = index;
            newWrite.dstSet = descriptorSet;
            newWrite.dstArrayElement = 0;
            newWrite.pBufferInfo = nullptr;
            newWrite.pTexelBufferView = nullptr;
            if (binding.descriptorType == vk::DescriptorType::eUniformBuffer)
            {
                // For now bind the existing UBO
                newWrite.pBufferInfo = &passFrameData.vsUniform.descriptor;
                writes.push_back(newWrite);
            }
            else if (binding.descriptorType == vk::DescriptorType::eAccelerationStructureKHR)
            {
                for (auto& geom : vulkanPass.pass.models)
                {
                    auto itrGeom = vulkanPass.vulkanScene.models.find(geom);
                    if (itrGeom != vulkanPass.vulkanScene.models.end())
                    {
                        newWrite.pNext = &itrGeom->second->topLevelASDescriptor;
                        writes.push_back(newWrite);
                        break;
                    }
                }
            }
            else if (binding.descriptorType == vk::DescriptorType::eCombinedImageSampler || binding.descriptorType == vk::DescriptorType::eStorageImage)
            {
                // Map specific sampler in set to the correct slot
                // Based on name in shader.
                // layout (set = 1, binding = 1) uniform sampler2D samplerA;
                auto itrImage = imageInfos.find(itrMeta->second.name);
                if (itrImage != imageInfos.end())
                {
                    newWrite.pImageInfo = &itrImage->second;
                    writes.push_back(newWrite);
                }
                else
                {
                    scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not find surface to bind to: {}", itrMeta->second.name), itrMeta->second.shaderPath, itrMeta->second.line, itrMeta->second.range);
                    return;
                }
            }
            else if (binding.descriptorType == vk::DescriptorType::eStorageBuffer)
            {
                for (auto& geom : vulkanPass.pass.models)
                {
                    auto itrGeom = vulkanPass.vulkanScene.models.find(geom);
                    if (itrGeom != vulkanPass.vulkanScene.models.end())
                    {
                        if (Zest::string_tolower(itrMeta->second.name) == "vertices")
                        {
                            newWrite.pBufferInfo = &itrGeom->second->verticesDescriptor;
                            writes.push_back(newWrite);
                        }
                        else if (Zest::string_tolower(itrMeta->second.name) == "indices")
                        {
                            newWrite.pBufferInfo = &itrGeom->second->indicesDescriptor;
                            writes.push_back(newWrite);
                        }
                        else
                        {
                            scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not find storage buffer to bind to: {}", itrMeta->second.name), itrMeta->second.shaderPath, itrMeta->second.line, itrMeta->second.range);
                            return;
                        }
                    }
                }
            }
            else
            {
                scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Unbound descriptor : {}", itrMeta->second.name));
                return;
            }
        }

        if (!writes.empty())
        {
            ctx.device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}

// 1. Get Shader stages for this pass
// 2. Remember the pass stage info to catch validation errors
bool vulkan_pass_prepare_pipeline(VulkanContext& ctx, VulkanPassSwapFrameData& frameData)
{
    PROFILE_SCOPE(prepare_pipeline);

    // No need to recreate the geometry pipeline
    if (frameData.pipeline)
    {
        return true;
    }

    LOG_SCOPE(DBG, "Prepare Pipeline:");

    auto& vulkanScene = frameData.pVulkanPass->vulkanScene;
    auto& vulkanPass = *frameData.pVulkanPass;
    auto& pass = frameData.pVulkanPass->pass;
    auto& vulkanPassTargets = vulkan_pass_targets(ctx, frameData);

    // Get the shader stage info
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    std::map<fs::path, uint32_t> shaderPathToIndex;
    for (auto& shaderPath : pass.shaders)
    {
        auto itrStage = vulkanScene.shaderStages.find(shaderPath);
        if (itrStage != vulkanScene.shaderStages.end())
        {
            shaderStages.push_back(itrStage->second->shaderCreateInfo);
            shaderPathToIndex[shaderPath] = shaderStages.size() - 1;
        }
    }

    frameData.rayGroupCreateInfos.clear();
    for (auto& group : pass.shaderGroups)
    {
        vk::RayTracingShaderGroupCreateInfoKHR shaderGroup;
        switch (group->groupType)
        {
        case ShaderType::RayGroupGeneral:
            shaderGroup = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
            break;
        case ShaderType::RayGroupTriangles:
            shaderGroup = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
            break;
        case ShaderType::RayGroupProcedural:
            shaderGroup = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
            break;
        }

        for (auto& shader : group->shaders)
        {
            auto itrStage = vulkanScene.shaderStages.find(shader.second->path);
            if (itrStage != vulkanScene.shaderStages.end())
            {
                switch (shader.first)
                {
                case RayShaderType::Miss:
                    shaderGroup.setGeneralShader(shaderPathToIndex[shader.second->path]);
                    break;
                case RayShaderType::Ray_Gen:
                    shaderGroup.setGeneralShader(shaderPathToIndex[shader.second->path]);
                    break;
                case RayShaderType::Any_Hit:
                    shaderGroup.setAnyHitShader(shaderPathToIndex[shader.second->path]);
                    break;
                case RayShaderType::Closest_Hit:
                    shaderGroup.setClosestHitShader(shaderPathToIndex[shader.second->path]);
                    break;
                case RayShaderType::Intersection:
                    shaderGroup.setIntersectionShader(shaderPathToIndex[shader.second->path]);
                    break;
                case RayShaderType::Callable:
                    assert(!"Is this OK?");
                    shaderGroup.setGeneralShader(shaderPathToIndex[shader.second->path]);
                    break;
                }
            }
        }
        frameData.rayGroupCreateInfos.push_back(shaderGroup);
    }

    if (!shaderStages.empty())
    {
        auto layouts = frameData.descriptorSetLayouts | views::transform([](auto& p) { return p.second; }) | to<std::vector>();
        frameData.geometryPipelineLayout = ctx.device.createPipelineLayout({ {}, layouts });
        debug_set_pipelinelayout_name(ctx.device, frameData.geometryPipelineLayout, fmt::format("GeomPipeLayout: {}", frameData.debugName));
    }

    if (frameData.pVulkanPass->pass.passType == PassType::Standard)
    {
        frameData.pipeline = pipeline_create(ctx, g_vertexLayout, frameData.geometryPipelineLayout, vulkanPassTargets, shaderStages);
        debug_set_pipeline_name(ctx.device, frameData.pipeline, fmt::format("GeomPipe: {}", frameData.debugName));
        LOG(DBG, "Create GeometryPipe: " << frameData.pipeline);
    }
    else
    {
        if (!frameData.rayGroupCreateInfos.empty())
        {
            vk::RayTracingPipelineCreateInfoKHR createInfo;
            createInfo.setStages(shaderStages);
            createInfo.setGroups(frameData.rayGroupCreateInfos);
            createInfo.setMaxPipelineRayRecursionDepth(8);
            createInfo.setLayout(frameData.geometryPipelineLayout);
            frameData.pipeline = ctx.device.createRayTracingPipelineKHR(nullptr, nullptr, createInfo).value;

            if (frameData.pipeline)
            {
                const uint32_t handleSize = ctx.rayTracingPipelineProperties.shaderGroupHandleSize;
                const uint32_t handleSizeAligned = alignedSize(ctx.rayTracingPipelineProperties.shaderGroupHandleSize, ctx.rayTracingPipelineProperties.shaderGroupHandleAlignment);

                auto groupCount = frameData.rayGroupCreateInfos.size();
                auto handles = ctx.device.getRayTracingShaderGroupHandlesKHR<uint8_t>(frameData.pipeline, 0, groupCount, groupCount * handleSizeAligned);

                frameData.rayGenBindingTable = buffer_create(ctx, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::DeviceSize(handleSize), handles.data());
                frameData.missBindingTable = buffer_create(ctx, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::DeviceSize(handleSize), handles.data() + handleSizeAligned);
                frameData.hitBindingTable = buffer_create(ctx, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::DeviceSize(handleSize), handles.data() + handleSizeAligned * 2);

                debug_set_buffer_name(ctx.device, frameData.rayGenBindingTable.buffer, fmt::format("{}:{}", frameData.debugName, "RayGenBindingTable"));
                debug_set_buffer_name(ctx.device, frameData.missBindingTable.buffer, fmt::format("{}:{}", frameData.debugName, "MissBindingTable"));
                debug_set_buffer_name(ctx.device, frameData.hitBindingTable.buffer, fmt::format("{}:{}", frameData.debugName, "HitBindingTable"));
        
                debug_set_pipeline_name(ctx.device, frameData.pipeline, fmt::format("RayTracePipe: {}", frameData.debugName));
                LOG(DBG, "Create RayTracePipe: " << frameData.pipeline);
            }
        }
    }

    return true;
}

// Transition samplers to read, if they are not already
void vulkan_pass_transition_samplers(VulkanContext& ctx, VulkanPassSwapFrameData& passFrameData)
{
    PROFILE_SCOPE(transition_samplers);
    auto& vulkanPass = *passFrameData.pVulkanPass;

    for (auto& passSampler : passFrameData.pVulkanPass->pass.samplers)
    {
        auto pVulkanSurface = get_vulkan_surface(ctx, vulkanPass, passSampler);

        if (pVulkanSurface && pVulkanSurface->image)
        {
            surface_set_layout(ctx, passFrameData.commandBuffer, pVulkanSurface->image, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
}

// TODO: Cleanup; after rendering the pass data we are transitioning surfaces to readable for the 'Targets' view.
// I need a cleaner/simpler management of target/sampler transitions.
void vulkan_pass_make_targets_readable(VulkanContext& ctx, VulkanPassSwapFrameData& passFrameData)
{
    PROFILE_SCOPE(make_targets_readable);
    auto& vulkanPass = *passFrameData.pVulkanPass;
    auto& passTargets = vulkan_pass_targets(ctx, passFrameData);

    for (auto& pTarget : passTargets.orderedTargets)
    {
        if (pTarget && pTarget->pVulkanSurface->image)
        {
            if (!vulkan_format_is_depth(pTarget->pVulkanSurface->format))
            {
                surface_set_layout(ctx, passFrameData.commandBuffer, pTarget->pVulkanSurface->image, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }
    }
}

void vulkan_pass_submit(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    PROFILE_SCOPE(pass_submit);
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(ctx, passFrameData);

    LOG_SCOPE(DBG, "Pass Submit: " << passFrameData.debugName);

    // Clear
    // Targets are sorted by name, with the depth pushed on the end.
    std::vector<vk::ClearValue> clearValues;
    vk::ClearValue depthClearValue;
    bool haveDepth = false;
    for (auto& pTargetData : passTargets.orderedTargets)
    {
        if (vulkan_format_is_depth(pTargetData->pVulkanSurface->format))
        {
            depthClearValue = vk::ClearDepthStencilValue{ 1.0f, 0 };
            haveDepth = true;
        }
        else
        {
            clearValues.push_back(clear_color(vulkanPass.pass.clearColor));
        }
    }

    if (haveDepth)
    {
        clearValues.push_back(depthClearValue);
    }

    // Draw geometry
    vk::RenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.renderPass = passTargets.renderPass;
    renderPassBeginInfo.framebuffer = passTargets.frameBuffer;
    renderPassBeginInfo.renderArea.extent.width = passTargets.targetSize.x;
    renderPassBeginInfo.renderArea.extent.height = passTargets.targetSize.y;
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();

    auto rect = passTargets.targetSize;

    auto& cmd = passFrameData.commandBuffer;
    debug_begin_region(cmd, passFrameData.debugName, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    if (passFrameData.pVulkanPass->pass.passType == PassType::Standard)
    {
        cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        cmd.setViewport(0, viewport(glm::uvec2(rect.x, rect.y)));
        cmd.setScissor(0, rect2d(glm::uvec2(rect.x, rect.y)));
        if (!passFrameData.descriptorSets.empty())
        {
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, passFrameData.geometryPipelineLayout, 0, passFrameData.descriptorSets, {});
        }

        if (passFrameData.pipeline)
        {
            // Graphics Pipe
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, passFrameData.pipeline);

            for (auto& geom : vulkanPass.pass.models)
            {
                auto itrGeom = vulkanPass.vulkanScene.models.find(geom);
                if (itrGeom != vulkanPass.vulkanScene.models.end())
                {
                    auto pVulkanGeom = itrGeom->second;
                    cmd.bindVertexBuffers(0, pVulkanGeom->vertices.buffer, { 0 });
                    cmd.bindIndexBuffer(pVulkanGeom->indices.buffer, 0, vk::IndexType::eUint32);
                    cmd.drawIndexed(pVulkanGeom->indexCount, 1, 0, 0, 0);
                }
            }
        }

        cmd.endRenderPass();

        vulkan_pass_make_targets_readable(ctx, passFrameData);
    }
    else if (passFrameData.pVulkanPass->pass.passType == PassType::RayTracing)
    {
        if (!passFrameData.descriptorSets.empty())
        {
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, passFrameData.geometryPipelineLayout, 0, passFrameData.descriptorSets, {});
        }
        // RT pipe
        cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, passFrameData.pipeline);

        const uint32_t handleSize = ctx.rayTracingPipelineProperties.shaderGroupHandleSize;
        const uint32_t handleSizeAligned = alignedSize(ctx.rayTracingPipelineProperties.shaderGroupHandleSize, ctx.rayTracingPipelineProperties.shaderGroupHandleAlignment);

        VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry;
        raygenShaderSbtEntry.deviceAddress = passFrameData.rayGenBindingTable.deviceAddress.deviceAddress;
        raygenShaderSbtEntry.stride = handleSizeAligned;
        raygenShaderSbtEntry.size = handleSizeAligned;

        VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
        missShaderSbtEntry.deviceAddress = passFrameData.missBindingTable.deviceAddress.deviceAddress;
        missShaderSbtEntry.stride = handleSizeAligned;
        missShaderSbtEntry.size = handleSizeAligned;

        VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
        hitShaderSbtEntry.deviceAddress = passFrameData.hitBindingTable.deviceAddress.deviceAddress;
        hitShaderSbtEntry.stride = handleSizeAligned;
        hitShaderSbtEntry.size = handleSizeAligned;

        VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

        cmd.traceRaysKHR(
            raygenShaderSbtEntry,
            missShaderSbtEntry,
            hitShaderSbtEntry,
            callableShaderSbtEntry,
            passTargets.targetSize.x,
            passTargets.targetSize.y,
            1);

        vulkan_pass_make_targets_readable(ctx, passFrameData);
    }

    debug_end_region(cmd);

    for (auto& pTargetData : passTargets.orderedTargets)
    {
        pTargetData->pVulkanSurface->pSurface->rendered = true;
    }

    /*
    if (passTargets.depth)
    {
        passTargets.depth->pSurface->rendered = true;
    }
    */

    passFrameData.commandBuffer.end();
    passFrameData.inFlight = true;

    LOG(DBG, "Submit CommandBuffer: " << passFrameData.commandBuffer << ", Fence: " << &passFrameData.fence << ", TID: " << std::this_thread::get_id());

    LOG(DBG, "Submit Pass");
    context_get_queue(ctx).submit(vk::SubmitInfo{ 0, nullptr, nullptr, 1, &passFrameData.commandBuffer }, passFrameData.fence);
}

bool vulkan_pass_draw(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    PROFILE_SCOPE(pass_draw);
    // Data for rendering the pass at the current swap frame
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(ctx, passFrameData);
    auto& scene = vulkanPass.pass.scene;

    passFrameData.pVulkanPass = &vulkanPass;
    passFrameData.debugName = fmt::format("{}:I{}", vulkanPass.pass.name, ctx.mainWindowData.frameIndex);
    passTargets.pFrameData = &passFrameData;

    LOG_SCOPE(DBG, "Pass Draw: " << passFrameData.debugName << " Global Frame: " << Scene::GlobalFrameCount);

    // Wait for the fence the last time we drew with this pass information
    vulkan_pass_wait(ctx, passFrameData);

    // Get command buffers ready if necessary
    vulkan_pass_prepare_command_buffers(ctx, passFrameData);

    // Tell the validation which shaders are currently being used, for catching debug errors
    validation_set_shaders(vulkanPass.pass.shaders);

    // Get our targets ready
    vulkan_pass_prepare_targets(ctx, passFrameData);

    // Samplers/ Surfaces
    vulkan_pass_prepare_surfaces(ctx, passFrameData);

    // Renderpasses
    vulkan_pass_prepare_renderpass(ctx, passTargets);

    // Uniform buffers
    vulkan_pass_prepare_uniforms(ctx, vulkanPass);

    // Prepare bindings for this pass; may not have to recreate
    if (vulkan_pass_build_descriptors(ctx, vulkanPass))
    {
        // Graphics pipeline
        if (vulkan_pass_prepare_pipeline(ctx, passFrameData))
        {
            // Build the actual descriptors, new each time
            vulkan_pass_set_descriptors(ctx, vulkanPass);
        }
    }

    // Validation layer may set an error, meaning this scene is not valid!
    // audio_destroy it, and reset the error trigger
    if (validation_get_error_state() || !scene.valid)
    {
        LOG(DBG, "!PASS INVALID!");
        vulkan_scene_destroy(ctx, vulkanPass.vulkanScene);
        validation_clear_error_state();
        return false;
    }

    // Make sure that samplers can be read by the shaders they are bound to
    vulkan_pass_transition_samplers(ctx, passFrameData);

    // Submit the draw
    vulkan_pass_submit(ctx, vulkanPass);

    // Not validating against these shaders
    validation_set_shaders({});

    if (validation_get_error_state() || !scene.valid)
    {
        LOG(DBG, "!PASS INVALID AFTER DRAW!");
        vulkan_scene_destroy(ctx, vulkanPass.vulkanScene);
        validation_clear_error_state();
        return false;
    }

    return true;
}

} // namespace vulkan

