#include <fmt/format.h>
#include <atomic>

#include <vklive/logger/logger.h>

#include <vklive/file/runtree.h>
#include <vklive/time/timer.h>
#include <vklive/validation.h>

#include <vklive/vulkan/vulkan_command.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_descriptor.h>
#include <vklive/vulkan/vulkan_pipeline.h>
#include <vklive/vulkan/vulkan_reflect.h>
#include <vklive/vulkan/vulkan_scene.h>
#include <vklive/vulkan/vulkan_shader.h>
#include <vklive/vulkan/vulkan_uniform.h>
#include <vklive/vulkan/vulkan_utils.h>

#include <vklive/audio/audio.h>

namespace vulkan
{

// Vertex layout for this example
VertexLayout g_vertexLayout{ {
    Component::VERTEX_COMPONENT_POSITION,
    Component::VERTEX_COMPONENT_UV,
    Component::VERTEX_COMPONENT_COLOR,
    Component::VERTEX_COMPONENT_NORMAL,
} };

std::string debug_pass_name(VulkanPass& pass, const std::string& str)
{
    return fmt::format("Pass::{}::{}", pass.pPass->name, str);
}

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

void vulkan_scene_create_renderpass(VulkanContext& ctx, VulkanScene& scene, VulkanPass& pass)
{
    std::vector<vk::Format> colorFormats;
    vk::Format depthFormat = vk::Format::eUndefined;

    /*
    for (auto& target : pass.pPass->targets)
    {
        auto itrSurface = scene.surfaces.find(target);
        if (itrSurface != scene.surfaces.end())
        {
            colorFormats.push_back(itrSurface->second->image.format);
        }
        else
        {
            assert(!"TODO");
        }
    }

    auto itrDepth = scene.surfaces.find(pass.pPass->depth);
    if (itrDepth != scene.surfaces.end())
    {
        depthFormat = itrDepth->second->image.format;
    }
    */

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
        attachments[i].loadOp = pass.pPass->hasClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
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
    pass.renderPass = ctx.device.createRenderPass(renderPassInfo);
    debug_set_renderpass_name(ctx.device, pass.renderPass, debug_pass_name(pass, "RenderPass"));
}

std::vector<VulkanShader*> pass_gather_shaders(VulkanScene& scene, VulkanPass& pass)
{
    std::vector<VulkanShader*> shaders;
    for (auto& shader : pass.pPass->shaders)
    {
        auto itrShader = scene.shaderStages.find(shader);
        if (itrShader == scene.shaderStages.end())
        {
            return {};
        }
        shaders.push_back(itrShader->second.get());
    }
    return shaders;
}

std::map<uint32_t, VulkanBindingSet> shaders_merge_descriptors(Pass& pass, const std::vector<VulkanShader*>& shaders)
{
    std::map<uint32_t, VulkanBindingSet> bindingSets;
    for (auto& pShader : shaders)
    {
        for (auto& [set, bindings] : pShader->bindingSets)
        {
            bindingSets[set].bindings.merge(bindings.bindings);
            bindingSets[set].bindingMeta.merge(bindings.bindingMeta);
        }
    }

    // Merge the stage flags for matching bindings
    // TODO: Is this all we need to do here?
    for (auto& [setA, bindingSetA] : bindingSets)
    {
        auto& bindingsA = bindingSetA.bindings;
        for (auto& pShader : shaders)
        {
            for (auto& bindingsB : pShader->bindingSets[setA].bindings)
            {
                auto itr = bindingsA.find(bindingsB.first);
                if (itr != bindingsA.end())
                {
                    itr->second.stageFlags |= bindingsB.second.stageFlags;
                }
            }
        }
    }

    LOG(DBG, fmt::format("Pass {}, Descriptors:", pass.name));
    for (auto& [set, bindings] : bindingSets)
    {
        LOG(DBG, "Set: " << set);
        for (auto& [index, binding] : bindings.bindings)
        {
            LOG(DBG, fmt::format("{} {} (Count: {}) Flags: {}", index, ToStringDescriptorType((SpvReflectDescriptorType)binding.descriptorType), binding.descriptorCount, (VkShaderStageFlags)binding.stageFlags));
        }
    }

    return bindingSets;
}

void vulkan_scene_init(VulkanContext& ctx, Scene& scene)
{
    // Already has errors, we can't build vulkan info from it.
    if (!scene.errors.empty() || !fs::exists(scene.root) || !scene.valid)
    {
        // Remove any old state
        vulkan_scene_destroy(ctx, scene);
        return;
    }

    // Ensure we start clean (after scene.valid check!)
    vulkan_scene_destroy(ctx, scene);

    // Start assuming valid state
    scene.valid = true;

    auto spVulkanScene = std::make_shared<VulkanScene>(&scene);
    ctx.mapVulkanScene[&scene] = spVulkanScene;

    descriptor_init(ctx, spVulkanScene->descriptorCache);

    // First, load the models
    for (auto& [path, pGeom] : scene.geometries)
    {
        auto spVulkanGeometry = std::make_shared<VulkanGeometry>(pGeom.get());

        // The geometry may be user geom or a model loaded from run tree; so just check it is available
        fs::path loadPath;
        if (pGeom->type == GeometryType::Model)
        {
            loadPath = pGeom->path;
        }
        else if (pGeom->type == GeometryType::Rect)
        {
            loadPath = runtree_find_path("models/quad.obj");
            if (loadPath.empty())
            {
                scene_report_error(scene, fmt::format("Could not load default asset: {}", "runtree/models/quad.obj"));
                continue;
            }
        }

        model_load(ctx, spVulkanGeometry->model, loadPath.string(), g_vertexLayout, pGeom->loadScale);

        // Success?
        if (spVulkanGeometry->model.vertexData.empty())
        {
            auto txt = fmt::format("Could not load model: {}", loadPath.string());
            if (!spVulkanGeometry->model.errors.empty())
            {
                txt += "\n" + spVulkanGeometry->model.errors;
            }
            scene_report_error(scene, txt);
        }
        else
        {
            // Store at original path, even though we may have subtituted geometry for preset paths
            spVulkanScene->geometries[path] = spVulkanGeometry;
        }
    }

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (auto& [path, pShader] : scene.shaders)
    {
        auto pVulkanShader = shader_create(ctx, scene, *pShader);

        if (pVulkanShader && pVulkanShader->shaderCreateInfo.module)
        {
            spVulkanScene->shaderStages[path] = pVulkanShader;
            pVulkanShader->shaderCreateInfo.pName = "main";
        }
        else
        {
            scene_report_error(scene, fmt::format("Could not create shader: {}", path.filename().string()));
        }
    }

    // Walk the surfaces
    for (auto& [name, spSurface] : scene.surfaces)
    {
        // TODO: Fixed sized surfaces/reload
        auto spVulkanSurface = std::make_shared<VulkanSurface>(spSurface.get());
        spVulkanScene->surfaces[name] = spVulkanSurface;
    }

    // Walk the passes
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

        auto shaders = pass_gather_shaders(*spVulkanScene, *spVulkanPass);
        spVulkanPass->mergedBindingSets = shaders_merge_descriptors(*spPass, shaders);

        spVulkanScene->passes[name] = spVulkanPass;
    }

    // Cleanup
    if (!scene.valid)
    {
        vulkan_scene_destroy(ctx, scene);
    }
}

void vulkan_scene_wait(VulkanContext& ctx, VulkanScene* pVulkanScene)
{
    if (pVulkanScene->inFlight)
    {
        const uint64_t FenceTimeout = 100000000;
        while (vk::Result::eTimeout == ctx.device.waitForFences(pVulkanScene->fence, VK_TRUE, FenceTimeout))
            ;
        pVulkanScene->commandBuffer.reset();
        pVulkanScene->inFlight = false;
        ctx.device.resetFences(pVulkanScene->fence);
    }
}

bool vulkan_scene_build_bindings(VulkanContext& ctx, VulkanScene& vulkanScene, VulkanPass& vulkanPass)
{
    // Build pointers to image infos for later
    std::map<std::string, vk::DescriptorImageInfo> imageInfos;
    for (auto& sampler : vulkanPass.pPass->samplers)
    {
        auto itrSurface = vulkanScene.surfaces.find(sampler);
        if (itrSurface != vulkanScene.surfaces.end())
        {
            auto pSurface = itrSurface->second;
            if (pSurface->image && pSurface->view)
            {
                vk::DescriptorImageInfo desc_image;
                desc_image.sampler = pSurface->sampler;
                desc_image.imageView = pSurface->view;
                desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                imageInfos[sampler] = desc_image;
            }
        }
    }
    
    for (auto& [set, bindingSet] : vulkanPass.mergedBindingSets)
    {
        LOG(DBG, "Set: " << set);
        std::vector<vk::DescriptorSetLayoutBinding> flatBindingList;
        std::vector<vk::WriteDescriptorSet> writes;

        for (auto& [index, binding] : bindingSet.bindings)
        {
            auto itrMeta = bindingSet.bindingMeta.find(index);
            if (itrMeta == bindingSet.bindingMeta.end())
            {
                // TODO: Is this an Error? I think it might be
                return false;
            }

            flatBindingList.push_back(binding);

            // TODO: Binding count.  When is there more than 1 specified? An array in the shader?
            vk::WriteDescriptorSet newWrite{};
            newWrite.pNext = nullptr;
            newWrite.descriptorCount = 1;
            newWrite.descriptorType = binding.descriptorType;
            newWrite.dstBinding = index;
            newWrite.dstSet = nullptr;
            newWrite.dstArrayElement = 0;
            newWrite.pBufferInfo = nullptr;
            newWrite.pTexelBufferView = nullptr;
            if (binding.descriptorType == vk::DescriptorType::eUniformBuffer)
            {
                // For now bind the existing UBO
                newWrite.pBufferInfo = &vulkanPass.vsUniform.descriptor;
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
                    scene_report_error(*vulkanScene.pScene, fmt::format("Could not find surface to bind to: {}", itrMeta->second.name), itrMeta->second.shaderPath, itrMeta->second.line, itrMeta->second.range);
                    return false;
                }
            }
        }

        // build layout first
        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.pNext = nullptr;
        layoutInfo.pBindings = flatBindingList.data();
        layoutInfo.bindingCount = static_cast<uint32_t>(flatBindingList.size());

        bindingSet.descriptorLayout = descriptor_create_layout(ctx, vulkanScene.descriptorCache, layoutInfo);

        // allocate descriptor
        bool success = descriptor_allocate(ctx, vulkanScene.descriptorCache, &bindingSet.descriptorSet, bindingSet.descriptorLayout);
        if (!success)
        {
            scene_report_error(*vulkanScene.pScene, fmt::format("Could not allocate descriptor"));
            return false;
        };

        if (!writes.empty())
        {
            for (vk::WriteDescriptorSet& w : writes)
            {
                w.dstSet = bindingSet.descriptorSet;
            }

            ctx.device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }

        debug_set_descriptorsetlayout_name(ctx.device, bindingSet.descriptorLayout, debug_pass_name(vulkanPass, "Layout"));
        debug_set_descriptorset_name(ctx.device, bindingSet.descriptorSet, debug_pass_name(vulkanPass, "DescriptorSet"));
    }

    // Store these for easy setting later
    vulkanPass.descriptorSetLayouts.clear();
    vulkanPass.descriptorSets.clear();
    for (auto& [set, bindingSet] : vulkanPass.mergedBindingSets)
    {
        vulkanPass.descriptorSetLayouts.push_back(bindingSet.descriptorLayout);
        vulkanPass.descriptorSets.push_back(bindingSet.descriptorSet);
    }

    return true;
}

void vulkan_scene_prepare(VulkanContext& ctx, RenderContext& renderContext, Scene& scene)
{
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    if (!pVulkanScene)
    {
        return;
    }

    if (!pVulkanScene->commandBuffer)
    {
        pVulkanScene->fence = ctx.device.createFence(vk::FenceCreateInfo());
        pVulkanScene->commandPool = ctx.device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, ctx.graphicsQueue));
        pVulkanScene->commandBuffer = ctx.device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pVulkanScene->commandPool, vk::CommandBufferLevel::ePrimary, 1))[0];

        debug_set_commandpool_name(ctx.device, pVulkanScene->commandPool, "Scene:CommandPool");
        debug_set_commandbuffer_name(ctx.device, pVulkanScene->commandBuffer, "Scene:CommandBuffer");
        debug_set_fence_name(ctx.device, pVulkanScene->fence, "Scene:Fence");
    }

    bool targetsChanged = false;
    // Resize/create rendertargets
    for (auto& [name, pVulkanSurface] : pVulkanScene->surfaces)
    {
        auto pSurface = pVulkanSurface->pSurface;
        pSurface->rendered = false;

        if (pSurface->name == "AudioAnalysis")
        {
            bool surfaceChanged = false;
            surface_update_from_audio(ctx, *pVulkanSurface, surfaceChanged);
            if (surfaceChanged)
            {
                targetsChanged = true;
            }
        }
        else if (pSurface->path.empty())
        {
            auto size = pSurface->size;
            if (size == glm::uvec2(0, 0))
            {
                size = glm::uvec2(pSurface->scale.x * renderContext.frameBufferSize.x, pSurface->scale.y * renderContext.frameBufferSize.y);
            }

            if (size != pVulkanSurface->currentSize)
            {
                targetsChanged = true;

                vulkan_scene_wait(ctx, pVulkanScene);
                surface_destroy(ctx, *pVulkanSurface);

                // Update to latest, even if we fail
                pVulkanSurface->currentSize = size;

                if (size != glm::uvec2(0, 0))
                {
                    if (format_is_depth(pSurface->format))
                    {
                        surface_create_depth(ctx, *pVulkanSurface, size, vulkan_scene_format_to_vulkan(pSurface->format), true, pSurface->name);
                    }
                    else
                    {
                        surface_create(ctx, *pVulkanSurface, size, vulkan_scene_format_to_vulkan(pSurface->format), true, pSurface->name);
                        pVulkanSurface->sampler = ctx.device.createSampler(vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear));
                        debug_set_sampler_name(ctx.device, pVulkanSurface->sampler, pSurface->name + "::Sampler");
                    }
                }
            }
        }
        else
        {
            if (pVulkanSurface->allocationState == VulkanAllocationState::Init)
            {
                auto file = scene_find_asset(scene, pVulkanSurface->pSurface->path, AssetType::Texture);
                if (!file.empty())
                {
                    surface_create_from_file(ctx, *pVulkanSurface, file);
                }
                pVulkanSurface->allocationState = VulkanAllocationState::Loaded;
            }

        }
    }

    if (targetsChanged)
    {
        // Need to wait before resetting descriptors
        vulkan_scene_wait(ctx, pVulkanScene);

        // ctx.queue.waitIdle();
        descriptor_reset_pools(ctx, pVulkanScene->descriptorCache);
    }

    for (auto& [name, pVulkanPass] : pVulkanScene->passes)
    {
        if ((pVulkanPass->currentFrameBufferSize != renderContext.frameBufferSize) || targetsChanged)
        {
            pVulkanPass->currentFrameBufferSize = renderContext.frameBufferSize;

            pVulkanPass->colorImages.clear();
            pVulkanPass->pDepthImage = nullptr;

            std::vector<glm::uvec2> sizes;
            // Figure out which surfaces we are using
            for (auto& target : pVulkanPass->pPass->targets)
            {
                auto itr = pVulkanScene->surfaces.find(target);
                if (itr != pVulkanScene->surfaces.end())
                {
                    auto pVulkanSurface = itr->second.get();
                    if (format_is_depth(itr->second->pSurface->format))
                    {
                        pVulkanPass->pDepthImage = pVulkanSurface;
                    }
                    else
                    {
                        pVulkanPass->colorImages.push_back(pVulkanSurface);

                        // TODO: We don't always need to setup for sampling if this target is never read.
                        if (!pVulkanSurface->samplerDescriptorSet)
                        {
                            surface_set_sampling(ctx, *pVulkanSurface);
                        }
                    }
                }
                else
                {
                    scene_report_error(scene, fmt::format("Could not find target: {}", target));
                }
            }

            auto pPass = pVulkanPass->pPass;
            auto pScene = pVulkanScene->pScene;
            auto checkSize = [&, pPass, pScene](auto img, auto& size) {
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
                        Message msg;
                        msg.text = "Target sizes don't match";
                        msg.path = pScene->sceneGraphPath;
                        msg.line = pPass->scriptTargetsLine;
                        msg.severity = MessageSeverity::Error;

                        // Any error invalidates the scene
                        scene.errors.push_back(msg);

                        vulkan_scene_destroy(ctx, scene);
                        return false;
                    }
                }
                return true;
            };

            glm::uvec2 size = glm::uvec2(0);
            for (auto& color : pVulkanPass->colorImages)
            {
                if (!checkSize(color, size))
                {
                    return;
                }
            }
            if (!checkSize(pVulkanPass->pDepthImage, size))
            {
                return;
            }

            pVulkanPass->targetSize = size;

            // Use render framebuffers for now
            vulkan::framebuffer_create(ctx, pVulkanPass->frameBuffer, pVulkanPass->colorImages, pVulkanPass->pDepthImage, pVulkanPass->renderPass);
            debug_set_framebuffer_name(ctx.device, pVulkanPass->frameBuffer.framebuffer, debug_pass_name(*pVulkanPass, "Framebuffer"));

            // Need to recreate the geometry pipeline
            if (pVulkanPass->geometryPipeline)
            {
                ctx.device.destroyPipeline(pVulkanPass->geometryPipeline);
            }

            // Get the shader stage info
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
            for (auto& shaderPath : pVulkanPass->pPass->shaders)
            {
                auto itrStage = pVulkanScene->shaderStages.find(shaderPath);
                if (itrStage != pVulkanScene->shaderStages.end())
                {
                    shaderStages.push_back(itrStage->second->shaderCreateInfo);
                }
            }

            validation_set_shaders(pVulkanPass->pPass->shaders);

            if (!shaderStages.empty())
            {
                if (!vulkan_scene_build_bindings(ctx, *pVulkanScene, *pVulkanPass))
                {
                    return;
                }

                if (pVulkanPass->geometryPipelineLayout)
                {
                    ctx.device.destroyPipelineLayout(pVulkanPass->geometryPipelineLayout);
                    pVulkanPass->geometryPipelineLayout = nullptr;
                }
                pVulkanPass->geometryPipelineLayout = ctx.device.createPipelineLayout({ {}, pVulkanPass->descriptorSetLayouts });
                debug_set_pipelinelayout_name(ctx.device, pVulkanPass->geometryPipelineLayout, debug_pass_name(*pVulkanPass, "PipelineLayout"));
            }

            validation_set_shaders({});

            // Create it
            /* TODO: Why does this break stuff?
            if (pVulkanPass->geometryPipeline)
            {
                ctx.device.destroyPipeline(pVulkanPass->geometryPipeline);
                pVulkanPass->geometryPipeline = nullptr;
            }
            */
            pVulkanPass->geometryPipeline = pipeline_create(ctx, g_vertexLayout, pVulkanPass->geometryPipelineLayout, pVulkanPass->renderPass, shaderStages);
            debug_set_pipeline_name(ctx.device, pVulkanPass->geometryPipeline, debug_pass_name(*pVulkanPass, "Pipeline"));
        }
    }

    // Copy the actual vertices to the GPU, if necessary.
    for (auto& [name, pVulkanGeom] : pVulkanScene->geometries)
    {
        model_stage(ctx, pVulkanGeom->model);
    }

    // Validation layer may set an error, meaning this scene is not valid!
    // audio_destroy it, and reset the error trigger
    if (validation_get_error_state() || !scene.valid)
    {
        vulkan_scene_destroy(ctx, scene);
        validation_clear_error_state();
    }
}

void vulkan_scene_destroy(VulkanContext& ctx, Scene& scene)
{
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    if (!pVulkanScene)
    {
        return;
    }

    ctx.device.waitIdle();

    descriptor_cleanup(ctx, pVulkanScene->descriptorCache);

    if (pVulkanScene->commandPool)
    {
        ctx.device.destroyFence(pVulkanScene->fence);
        ctx.device.destroyCommandPool(pVulkanScene->commandPool);
        pVulkanScene->commandPool = nullptr;
    }

    for (auto& [name, pVulkanSurface] : pVulkanScene->surfaces)
    {
        surface_destroy(ctx, *pVulkanSurface);
    }

    for (auto& [name, pShader] : pVulkanScene->shaderStages)
    {
        ctx.device.destroyShaderModule(pShader->shaderCreateInfo.module);
    }
    pVulkanScene->shaderStages.clear();

    // Delete the models
    for (auto& [name, pGeom] : pVulkanScene->geometries)
    {
        if (pGeom->model.vertices.buffer)
        {
            model_destroy(ctx, pGeom->model);
        }
    }
    pVulkanScene->geometries.clear();

    for (auto& [name, pVulkanPass] : pVulkanScene->passes)
    {
        framebuffer_destroy(ctx, pVulkanPass->frameBuffer);
        buffer_destroy(ctx, pVulkanPass->vsUniform);

        ctx.device.destroyRenderPass(pVulkanPass->renderPass);
        ctx.device.destroyPipeline(pVulkanPass->geometryPipeline);
        ctx.device.destroyPipelineLayout(pVulkanPass->geometryPipelineLayout);
    }
    pVulkanScene->passes.clear();

    // TODO: Delete scene objects
    ctx.mapVulkanScene.erase(&scene);

    scene.valid = false;
}

void vulkan_scene_render(VulkanContext& ctx, RenderContext& renderContext, Scene& scene)
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

        // Setup the camera for this pass
        // pVulkanPass->pPass->camera.orbitDelta = glm::vec2(4.0f, 0.0f);
        camera_set_film_size(pVulkanPass->pPass->camera, glm::ivec2(size));
        camera_pre_render(pVulkanPass->pPass->camera);

        glm::vec3 meshPos = glm::vec3(0.0f, 0.0f, 0.0f);
        auto elapsed = timer_get_elapsed_seconds(globalTimer);
        pVulkanPass->vsUBO.iTimeDelta = (pVulkanPass->vsUBO.iTime == 0.0f) ? 0.0f : elapsed - pVulkanPass->vsUBO.iTime;
        pVulkanPass->vsUBO.iTime = elapsed;
        pVulkanPass->vsUBO.iFrame = globalFrameCount;
        pVulkanPass->vsUBO.iFrameRate = elapsed != 0.0 ? (1.0f / elapsed) : 0.0;
        pVulkanPass->vsUBO.iGlobalTime = elapsed;
        pVulkanPass->vsUBO.iResolution = glm::vec4(size.x, size.y, 1.0, 0.0);
        pVulkanPass->vsUBO.iMouse = glm::vec4(0.0f); // TODO: Mouse

        // Audio
        auto& audioCtx = Audio::GetAudioContext();
        pVulkanPass->vsUBO.iSampleRate = audioCtx.audioDeviceSettings.sampleRate;
        auto channels = std::min(audioCtx.analysisChannels.size(), (size_t)2);
        for (int channel = 0; channel < channels; channel++)
        {
            // Lock free atomic
            pVulkanPass->vsUBO.iSpectrumBands[channel] = audioCtx.analysisChannels[channel]->spectrumBands;
        }

        // TODO: year, month, day, seconds since EPOCH
        pVulkanPass->vsUBO.iDate = glm::vec4(0.0f);

        // TODO: Based on input sizes; shouldn't be same as target necessarily.
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
        renderPassBeginInfo.framebuffer = pVulkanPass->frameBuffer.framebuffer;
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

            for (auto& col : pVulkanPass->colorImages)
            {
                col->pSurface->rendered = true;
            }

            if (pVulkanPassPtr->pDepthImage)
            {
                pVulkanPassPtr->pDepthImage->pSurface->rendered = true;
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

vk::Format vulkan_scene_format_to_vulkan(const Format& format)
{
    switch (format)
    {
    case Format::D32:
    case Format::Default_Depth:
        return vk::Format::eD32Sfloat;
    case Format::Default:
    case Format::R8G8B8A8UNorm:
        return vk::Format::eR8G8B8A8Unorm;
    }
    assert(!"Unknown format?");
    return vk::Format::eR8G8B8A8Unorm;
}

} // namespace vulkan
