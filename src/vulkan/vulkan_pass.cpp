#include <fmt/format.h>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include "vklive/logger/logger.h"
#include "vklive/time/timer.h"
#include "vklive/validation.h"

#include "vklive/vulkan/vulkan_pass.h"
#include "vklive/vulkan/vulkan_pipeline.h"
#include "vklive/vulkan/vulkan_render.h"
#include "vklive/vulkan/vulkan_uniform.h"
#include "vklive/vulkan/vulkan_utils.h"

#include "vklive/audio/audio.h"

using namespace ranges;

namespace vulkan
{

VulkanPassSwapFrameData& vulkan_pass_frame_data(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    return vulkanPass.passFrameData[ctx.mainWindowData.frameIndex];
}

VulkanPassTargets& vulkan_pass_targets(VulkanPassSwapFrameData& passFrameData)
{
    return passFrameData.passTargets[globalFrameCount % 2];
}

std::shared_ptr<VulkanPass> vulkan_pass_create(VulkanScene& vulkanScene, Pass& pass)
{
    auto spVulkanPass = std::make_shared<VulkanPass>(vulkanScene, pass);
    vulkanScene.passes[pass.name] = spVulkanPass;

    // Camera setup
    pass.camera.nearFar = glm::vec2(0.1f, 256.0f);
    camera_set_pos_lookat(pass.camera, glm::vec3(0.0f, 0.0f, 6.0f), glm::vec3(0.0f, 0.0f, 0.0f));

    return spVulkanPass;
}

void vulkan_pass_destroy(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    for (auto& [index, passData] : vulkanPass.passFrameData)
    {
        LOG(DBG, "Pass Destroy: " << passData.debugName);

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

        // Pipeline/graphics
        LOG(DBG, "Destroy GeometryPipe: " << passData.geometryPipeline);
        ctx.device.destroyPipeline(passData.geometryPipeline);
        ctx.device.destroyPipelineLayout(passData.geometryPipelineLayout);
        passData.geometryPipeline = nullptr;
        passData.geometryPipelineLayout = nullptr;
    }
}

// Wait for this pass to finish sending to the hardware
void vulkan_pass_wait(VulkanContext& ctx, VulkanPassSwapFrameData& passData)
{
    LOG(DBG, "Wait for pass: " << passData.debugName);
    if (passData.inFlight)
    {
        const uint64_t FenceTimeout = 100000000;
        while (vk::Result::eTimeout == ctx.device.waitForFences(passData.fence, VK_TRUE, FenceTimeout))
            ;
        // We can now re-use the command buffer
        passData.commandBuffer.reset();
        passData.inFlight = false;
        ctx.device.resetFences(passData.fence);

        LOG(DBG, "Reset fence: " << &passData.fence);
    }
    else
    {
        LOG(DBG, "Not in flight: " << &passData.fence);
    }
}

void vulkan_pass_wait_all(VulkanContext& ctx, VulkanScene& scene)
{
    for (auto& [name, vulkanPass] : scene.passes)
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
    auto pVulkanSurface = vulkan_scene_get_or_create_surface(vulkanScene, surfaceName, globalFrameCount, sampling);
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
            if (vulkanScene.audioSurfaceFrameGeneration != globalFrameCount || (pVulkanSurface->allocationState == VulkanAllocationState::Init))
            {
                vulkanScene.audioSurfaceFrameGeneration = globalFrameCount;

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
        LOG(DBG, "Resized: " << pVulkanSurface->debugName);

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

                // TODO: This is necessary to build a descriptor set for display in IMGUI.
                // Need a cleaner way to build this custom descriptor set for the IMGUI end, since the scene
                // render would only need this if the surface was sampled
                surface_set_sampling(ctx, *pVulkanSurface);
            }
        }

        pVulkanSurface->pSurface->rendered = false;
    }
    return pVulkanSurface;
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

    for (auto& [name, surface] : passTargets.targets)
    {
        if (!checkSize(surface, size))
        {
            // If sizes don't match, we are effectively broken and can't render
            return false;
        }

        if (checkForChanges(surface))
        {
            diff = true;
        }
    }

    if (!checkSize(passTargets.depth, size))
    {
        return false;
    }

    if (checkForChanges(passTargets.depth))
    {
        diff = true;
    }

    // If our pass targets have changed size, then we need to clean up the framebuffer, renderpass and geom pipe
    // TODO: This only handles resizes, not recreation?...
    if (diff)
    {
        LOG(DBG, "Targets different, cleaning up FB, RenderPass, Geom");

        // I think we can just wait for the pass here, since these are all pass-specific objects
        // Note that a previous pass may have changed targets, even if this one didn't.  So we need
        // to update our state here
        vulkan_pass_wait(ctx, *passTargets.pFrameData);

        // destroy the framebuffer since it depends on renderpass and targets
        if (passTargets.frameBuffer)
        {
            LOG(DBG, "Destroy Framebuffer: " << passTargets.frameBuffer);
            framebuffer_destroy(ctx, passTargets.frameBuffer);
            passTargets.frameBuffer = nullptr;
        }

        // Renderpass has the framebuffer, which we had to recreate, so recreate render pass
        if (passTargets.renderPass)
        {
            LOG(DBG, "Destroy RenderPass: " << passTargets.renderPass);
            ctx.device.destroyRenderPass(passTargets.renderPass);
            passTargets.renderPass = nullptr;
        }

        // Geom pipe uses the render pass
        if (passTargets.pFrameData && passTargets.pFrameData->geometryPipeline)
        {
            LOG(DBG, "Destroy Geometry Pipe: " << passTargets.pFrameData->geometryPipeline);
            ctx.device.destroyPipeline(passTargets.pFrameData->geometryPipeline);
            ctx.device.destroyPipelineLayout(passTargets.pFrameData->geometryPipelineLayout);
            passTargets.pFrameData->geometryPipeline = nullptr;
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

    for (auto& name : frameData.pVulkanPass->pass.samplers)
    {
        auto pSurface = get_vulkan_surface(ctx, *frameData.pVulkanPass, name, true);
        if (checkForChanges(pSurface))
        {
            LOG(DBG, "Sampler has changed, removing geometry pipe");

            // I think we can just wait for the pass here, since these are all pass-specific objects
            // Note that a previous pass may have changed targets, even if this one didn't.  So we need
            // to update our state here
            vulkan_pass_wait(ctx, *passTargets.pFrameData);

            // Geom pipe uses the render pass
            if (frameData.geometryPipeline)
            {
                LOG(DBG, "Destroy Geometry Pipe: " << frameData.geometryPipeline);
                ctx.device.destroyPipeline(frameData.geometryPipeline);
                ctx.device.destroyPipelineLayout(frameData.geometryPipelineLayout);
                passTargets.pFrameData->geometryPipeline = nullptr;
                passTargets.pFrameData->geometryPipelineLayout = nullptr;
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

        // TODO: Custom formats
        if (depthFormat == vk::Format::eUndefined && colorFormats.empty())
        {
            colorFormats.push_back(vk::Format::eR8G8B8A8Unorm);
            depthFormat = vk::Format::eD32Sfloat;
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

    LOG(DBG, "  FrameBuffer: " << passTargets.frameBuffer);
    LOG(DBG, "  RenderPass: " << passTargets.renderPass);
}

void vulkan_pass_dump_targets(VulkanPassTargets& passTargets)
{
    LOG(DBG, "Targets:");
    for (auto& [name, pVulkanSurface] : passTargets.targets)
    {
        LOG(DBG, "  Name: " << pVulkanSurface->debugName << " Target: " << pVulkanSurface->image);
    }

    if (passTargets.depth)
    {
        LOG(DBG, "  Name: " << passTargets.depth->debugName << " Target: " << passTargets.depth->debugName);
    }

    LOG(DBG, "  TargetSize: " << passTargets.targetSize.x << ", " << passTargets.targetSize.y);
}

void vulkan_pass_dump_samplers(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    if (vulkanPass.pass.samplers.empty())
    {
        return;
    }
    LOG(DBG, "Samplers:");
    for (auto& name : vulkanPass.pass.samplers)
    {
        auto pVulkanSurface = get_vulkan_surface(ctx, vulkanPass, name);
        if (pVulkanSurface)
        {
            LOG(DBG, "  Name: " << pVulkanSurface->debugName);
            LOG(DBG, "    Loaded: " << (pVulkanSurface->allocationState == VulkanAllocationState::Loaded));
            LOG(DBG, "    Image: " << pVulkanSurface->image);
            LOG(DBG, "    Extent: " << pVulkanSurface->extent.width << ", " << pVulkanSurface->extent.height << ", " << pVulkanSurface->extent.depth);
            LOG(DBG, "    MipLevels: " << pVulkanSurface->mipLevels);
            LOG(DBG, "    Format: " << to_string(pVulkanSurface->format));
        }
        else
        {
            LOG(DBG, "NOT Found: " << name);
        }
    }
}

void vulkan_pass_prepare_targets(VulkanContext& ctx, VulkanPassSwapFrameData& passFrameData)
{
    bool targetsChanged = false;

    // Flip-flop between 2 pages for this swap index; so we can read the target later
    auto& pass = passFrameData.pVulkanPass->pass;
    auto& vulkanScene = passFrameData.pVulkanPass->vulkanScene;
    auto& vulkanPassTargets = passFrameData.passTargets[globalFrameCount % 2];

    vulkanPassTargets.debugName = fmt::format("PassTargets:{}:{}", globalFrameCount % 2, passFrameData.debugName);

    LOG(DBG, "PrepareTargets: " << vulkanPassTargets.debugName);

    // Make sure all targets are relevent
    for (auto& surfaceName : pass.targets)
    {
        vulkanPassTargets.targets[surfaceName] = get_vulkan_surface(ctx, *passFrameData.pVulkanPass, surfaceName);
    }

    if (!pass.depth.empty())
    {
        vulkanPassTargets.depth = get_vulkan_surface(ctx, *passFrameData.pVulkanPass, pass.depth);
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
    auto& vulkanPassTargets = passFrameData.passTargets[globalFrameCount % 2];

    // Walk the surfaces
    for (auto& samplerName : pass.samplers)
    {
        auto pVulkanSurface = get_vulkan_surface(ctx, *passFrameData.pVulkanPass, samplerName, true);
        if (!pVulkanSurface)
        {
            scene_report_error(pass.scene, MessageSeverity::Error, fmt::format("Surface not found: ", samplerName), pass.scene.sceneGraphPath);
        }

        vulkan_pass_check_samplers(ctx, vulkanPassTargets, passFrameData);
    }

    vulkan_pass_dump_samplers(ctx, *passFrameData.pVulkanPass);
}

// Ensure we have setup the buffers for this pass
void vulkan_pass_prepare_uniforms(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(passFrameData);

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

    // Setup the camera for this pass
    // pVulkanPass->pPass->camera.orbitDelta = glm::vec2(4.0f, 0.0f);
    camera_set_film_size(vulkanPass.pass.camera, glm::ivec2(size));
    camera_pre_render(vulkanPass.pass.camera);

    auto elapsed = globalElapsedSeconds;
    glm::vec3 meshPos = glm::vec3(0.0f, 0.0f, 0.0f);
    ubo.iTimeDelta = (ubo.iTime == 0.0f) ? 0.0f : elapsed - ubo.iTime;
    ubo.iTime = elapsed;
    ubo.iFrame = globalFrameCount;
    ubo.iFrameRate = elapsed != 0.0 ? (1.0f / elapsed) : 0.0;
    ubo.iGlobalTime = elapsed;
    ubo.iResolution = glm::vec4(size.x, size.y, 1.0, 0.0);
    ubo.iMouse = glm::vec4(0.0f); // TODO: Mouse

    // Audio
    auto& audioCtx = Audio::GetAudioContext();
    ubo.iSampleRate = audioCtx.audioDeviceSettings.sampleRate;
    auto channels = std::min(audioCtx.analysisChannels.size(), (size_t)2);
    for (int channel = 0; channel < channels; channel++)
    {
        // Lock free atomic
        ubo.iSpectrumBands[channel] = audioCtx.analysisChannels[channel]->spectrumBands;
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
    ubo.view = camera_get_lookat(vulkanPass.pass.camera);
    ubo.projection = camera_get_projection(vulkanPass.pass.camera);
    ubo.modelViewProjection = ubo.projection * ubo.view * ubo.model;
    ubo.eye = glm::vec4(vulkanPass.pass.camera.position, 0.0f);

    // TODO: Should we stage using command buffer?  This is a direct write
    utils_copy_to_memory(ctx, passFrameData.vsUniform.memory, ubo);
}

void vulkan_pass_build_descriptors(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    VulkanScene& vulkanScene = vulkanPass.vulkanScene;
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(passFrameData);

    if (passFrameData.builtDescriptors)
    {
        return;
    }

    passFrameData.builtDescriptors = true;

    // Lookup the vulkan compiled shader stages that match the declared scene shaders
    auto& stages = vulkanPass.vulkanScene.shaderStages;

    auto f = [&](auto& p) { return stages.find(p) != stages.end(); };
    auto t = [&](auto& p) { return &stages.find(p)->second->bindingSets; };
    auto bindings = vulkanPass.pass.shaders | views::filter(f) | views::transform(t) | to<std::vector>();
    passFrameData.mergedBindingSets = bindings_merge(bindings);

    LOG(DBG, "  Pass: " << passFrameData.debugName << ", Merged Bindings:");
    bindings_dump(passFrameData.mergedBindingSets, 4);

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
                return;
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

            auto descriptorSetLayout = descriptor_create_layout(ctx, vulkan_descriptor_cache(ctx, vulkanScene), layoutInfo);
            debug_set_descriptorsetlayout_name(ctx.device, descriptorSetLayout, fmt::format("{}:{}", passFrameData.debugName, "Layout"));

            layout = descriptorSetLayout;
        }
    }
}

void vulkan_pass_set_descriptors(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    VulkanScene& vulkanScene = vulkanPass.vulkanScene;

    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(passFrameData);

    // Build pointers to image infos for later
    std::map<std::string, vk::DescriptorImageInfo> imageInfos;
    for (auto& sampler : vulkanPass.pass.samplers)
    {
        // TODO: Correct sampler; SurfaceKey needs to account for target ping/pong
        auto pVulkanSurface = get_vulkan_surface(ctx, vulkanPass, sampler, true);
        if (pVulkanSurface && pVulkanSurface->image && pVulkanSurface->view && pVulkanSurface->sampler)
        {
            vk::DescriptorImageInfo desc_image;
            desc_image.sampler = pVulkanSurface->sampler;
            desc_image.imageView = pVulkanSurface->view;
            desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            imageInfos[sampler] = desc_image;
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
        bool success = descriptor_allocate(ctx, vulkan_descriptor_cache(ctx, vulkanScene), &descriptorSet, layout);
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
            else if (binding.descriptorType == vk::DescriptorType::eCombinedImageSampler)
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
        }

        if (!writes.empty())
        {
            ctx.device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}

// 1. Get Shader stages for this pass
// 2. Remember the pass stage info to catch validation errors
void vulkan_pass_prepare_pipeline(VulkanContext& ctx, VulkanPassSwapFrameData& frameData)
{
    // No need to recreate the geometry pipeline
    if (frameData.geometryPipeline)
    {
        return;
    }

    auto& vulkanScene = frameData.pVulkanPass->vulkanScene;
    auto& vulkanPass = *frameData.pVulkanPass;
    auto& pass = frameData.pVulkanPass->pass;
    auto& vulkanPassTargets = vulkan_pass_targets(frameData);

    // Get the shader stage info
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (auto& shaderPath : pass.shaders)
    {
        auto itrStage = vulkanScene.shaderStages.find(shaderPath);
        if (itrStage != vulkanScene.shaderStages.end())
        {
            shaderStages.push_back(itrStage->second->shaderCreateInfo);
        }
    }

    if (!shaderStages.empty())
    {
        auto layouts = frameData.descriptorSetLayouts | views::transform([](auto& p) { return p.second; }) | to<std::vector>();
        frameData.geometryPipelineLayout = ctx.device.createPipelineLayout({ {}, layouts });
        debug_set_pipelinelayout_name(ctx.device, frameData.geometryPipelineLayout, fmt::format("GeomPipeLayout:" + frameData.debugName));
    }

    frameData.geometryPipeline = pipeline_create(ctx, g_vertexLayout, frameData.geometryPipelineLayout, vulkanPassTargets.renderPass, shaderStages);
    debug_set_pipeline_name(ctx.device, frameData.geometryPipeline, fmt::format("GeomPipe:" + frameData.debugName));

    LOG(DBG, "Create GeometryPipe: " << frameData.geometryPipeline);
}

// Transition samplers to read, if they are not already
void vulkan_pass_transition_samplers(VulkanContext& ctx, VulkanPassSwapFrameData& passFrameData)
{
    auto& vulkanPass = *passFrameData.pVulkanPass;

    for (auto& samplerName : passFrameData.pVulkanPass->pass.samplers)
    {
        auto pVulkanSurface = get_vulkan_surface(ctx, vulkanPass, samplerName, true);
        if (pVulkanSurface && pVulkanSurface->image)
        {
            surface_set_layout(ctx, passFrameData.commandBuffer, pVulkanSurface->image, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
}

void vulkan_pass_submit(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(passFrameData);

    // Clear
    vk::ClearValue clearValues[2];
    clearValues[0].color = clear_color(vulkanPass.pass.clearColor);
    clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

    // Draw geometry
    vk::RenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.renderPass = passTargets.renderPass;
    renderPassBeginInfo.framebuffer = passTargets.frameBuffer;
    renderPassBeginInfo.renderArea.extent.width = passTargets.targetSize.x;
    renderPassBeginInfo.renderArea.extent.height = passTargets.targetSize.y;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    auto rect = passTargets.targetSize;

    auto& cmd = passFrameData.commandBuffer;
    debug_begin_region(cmd, passFrameData.debugName, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    cmd.setViewport(0, viewport(glm::uvec2(rect.x, rect.y)));
    cmd.setScissor(0, rect2d(glm::uvec2(rect.x, rect.y)));
    if (!passFrameData.descriptorSets.empty())
    {
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, passFrameData.geometryPipelineLayout, 0, passFrameData.descriptorSets, {});
    }
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, passFrameData.geometryPipeline);

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

    cmd.endRenderPass();
    debug_end_region(cmd);

    for (auto& col : passTargets.targets)
    {
        col.second->pSurface->rendered = true;
    }

    if (passTargets.depth)
    {
        passTargets.depth->pSurface->rendered = true;
    }

    passFrameData.commandBuffer.end();
    passFrameData.inFlight = true;

    LOG(DBG, "Submit CommandBuffer: " << passFrameData.commandBuffer << ", Fence: " << &passFrameData.fence);

    ctx.queue.submit(vk::SubmitInfo{ 0, nullptr, nullptr, 1, &passFrameData.commandBuffer }, passFrameData.fence);
}

bool vulkan_pass_draw(VulkanContext& ctx, VulkanPass& vulkanPass)
{
    // Data for rendering the pass at the current swap frame
    auto& passFrameData = vulkan_pass_frame_data(ctx, vulkanPass);
    auto& passTargets = vulkan_pass_targets(passFrameData);

    passFrameData.frameIndex = ctx.mainWindowData.frameIndex;
    passFrameData.pVulkanPass = &vulkanPass;
    passFrameData.debugName = fmt::format("{}:{}", vulkanPass.pass.name, ctx.mainWindowData.frameIndex);
    passTargets.pFrameData = &passFrameData;

    LOG(DBG, "");
    LOG(DBG, "PassBegin: " << passFrameData.debugName << " Frame: " << ctx.mainWindowData.frameIndex << " Global Frame: " << globalFrameCount);

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
    vulkan_pass_build_descriptors(ctx, vulkanPass);

    // Graphics pipeline
    vulkan_pass_prepare_pipeline(ctx, passFrameData);

    // Build the actual descriptors, new each time
    vulkan_pass_set_descriptors(ctx, vulkanPass);

    // Not validating against these shaders
    validation_set_shaders({});

    // Validation layer may set an error, meaning this scene is not valid!
    // audio_destroy it, and reset the error trigger
    auto& scene = vulkanPass.pass.scene;
    if (validation_get_error_state() || !scene.valid)
    {
        LOG(DBG, "!PASS INVALID!");
        vulkan_scene_destroy(ctx, vulkanPass.vulkanScene);
        validation_clear_error_state();
        return false;
    }

    LOG(DBG, "PassEnd: " << passFrameData.debugName << " Frame: " << ctx.mainWindowData.frameIndex << " Global Frame: " << globalFrameCount << "\n");
    LOG(DBG, "");

    // Make sure that samplers can be read by the shaders they are bound to
    vulkan_pass_transition_samplers(ctx, passFrameData);

    // Submit the draw
    vulkan_pass_submit(ctx, vulkanPass);

    return true;
}

} // namespace vulkan
