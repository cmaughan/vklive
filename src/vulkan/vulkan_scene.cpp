#include <fmt/format.h>

#include <vklive/file/runtree.h>
#include <vklive/time/timer.h>
#include <vklive/validation.h>

#include <vklive/vulkan/vulkan_command.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_pipeline.h>
#include <vklive/vulkan/vulkan_scene.h>
#include <vklive/vulkan/vulkan_shader.h>
#include <vklive/vulkan/vulkan_uniform.h>
#include <vklive/vulkan/vulkan_utils.h>

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

void scene_init(VulkanContext& ctx, Scene& scene)
{
    // Ensure we start clean
    scene_destroy(ctx, scene);

    // Already has errors, we can't build vulkan info from it.
    if (!scene.errors.empty() || !scene.valid)
    {
        scene.valid = false;
        return;
    }

    auto spVulkanScene = std::make_shared<VulkanScene>(&scene);
    ctx.mapVulkanScene[&scene] = spVulkanScene;

    if (!fs::exists(scene.root))
    {
        scene.valid = false;
        return;
    }

    // Start assuming valid state
    scene.valid = true;

    auto reportError = [&](auto txt) {
        Message msg;
        msg.text = txt;
        msg.path = spVulkanScene->pScene->sceneGraphPath;
        msg.line = -1;
        msg.severity = MessageSeverity::Error;

        // Any error invalidates the scene
        scene.errors.push_back(msg);
        scene.valid = false;
    };

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
                reportError(fmt::format("Could not load default asset: {}", "runtree/models/quad.obj"));
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
            reportError(txt);
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
        auto pVulkanShader = std::make_shared<VulkanShader>(pShader.get());

        if (pShader->path.extension().string() == ".vert")
        {
            pVulkanShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
        }
        else if (pShader->path.extension().string() == ".frag")
        {
            pVulkanShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
        }
        else if (pShader->path.extension().string() == ".geom")
        {
            pVulkanShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eGeometry;
        }
        else
        {
            reportError(fmt::format("Unknown shader type: {}", path.filename().string()));
            continue;
        }

        pVulkanShader->shaderCreateInfo.module = shader_create(ctx, pShader->path, scene.errors);
        pVulkanShader->shaderCreateInfo.pName = "main";

        if (pVulkanShader->shaderCreateInfo.module)
        {
            spVulkanScene->shaderStages[path] = pVulkanShader;
        }
        else
        {
            reportError(fmt::format("Could not create shader: {}", path.filename().string()));
        }
    }

    // Walk the passes
    for (auto& [name, spPass] : scene.passes)
    {
        auto spVulkanPass = std::make_shared<VulkanPass>(spPass.get());

        // TODO
        for (auto& colorTarget : spPass->targets)
        {
        }

        // Renderpass for where this pass draws (the targets)
        vulkan_scene_create_renderpass(ctx, *spVulkanScene, *spVulkanPass);

        // Camera setup
        spPass->camera.nearFar = glm::vec2(0.1f, 256.0f);
        camera_set_pos_lookat(spPass->camera, glm::vec3(0.0f, 0.0f, 6.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        // Uniform Buffer setup
        spVulkanPass->vsUniform = uniform_create(ctx, spVulkanPass->vsUBO);
        debug_set_buffer_name(ctx.device, spVulkanPass->vsUniform.buffer, debug_pass_name(*spVulkanPass, "Uniforms"));
        debug_set_devicememory_name(ctx.device, spVulkanPass->vsUniform.memory, debug_pass_name(*spVulkanPass, "UniformMemory"));

        // Descriptor
        // Textured quad pipeline layout
        // DescriptorSet:
        // UNIFORM
        std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{
            // Binding 0 : Vertex shader uniform buffer
            { 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eGeometry } //,
            // Binding 1 : Fragment shader image sampler
            //{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
            // Binding 2 : Fragment shader image sampler
            //{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
        };

        spVulkanPass->descriptorSetLayout = ctx.device.createDescriptorSetLayout({ {}, (uint32_t)setLayoutBindings.size(), setLayoutBindings.data() });
        spVulkanPass->geometryPipelineLayout = ctx.device.createPipelineLayout({ {}, 1, &spVulkanPass->descriptorSetLayout });

        debug_set_descriptorsetlayout_name(ctx.device, spVulkanPass->descriptorSetLayout, debug_pass_name(*spVulkanPass, "DescriptorSetLayout"));
        debug_set_pipelinelayout_name(ctx.device, spVulkanPass->geometryPipelineLayout, debug_pass_name(*spVulkanPass, "PipelineLayout"));

        vk::DescriptorSetAllocateInfo allocInfo{ ctx.descriptorPool, 1, &spVulkanPass->descriptorSetLayout };
        spVulkanPass->descriptorSet = ctx.device.allocateDescriptorSets(allocInfo)[0];
        debug_set_descriptorset_name(ctx.device, spVulkanPass->descriptorSet, debug_pass_name(*spVulkanPass, "DescriptorSet"));

        // DecriptorSet
        // UNIFORM: vsUniform.descriptor
        std::vector<vk::WriteDescriptorSet> offscreenWriteDescriptorSets{
            // Binding 0 : Vertex shader uniform buffer
            { spVulkanPass->descriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &spVulkanPass->vsUniform.descriptor },
        };
        ctx.device.updateDescriptorSets(offscreenWriteDescriptorSets, {});

        spVulkanScene->passes[name] = spVulkanPass;
    }

    // Cleanup
    if (!scene.valid)
    {
        scene_destroy(ctx, scene);
    }
}

void scene_prepare(VulkanContext& ctx, RenderContext& renderContext, Scene& scene)
{
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    if (!pVulkanScene)
    {
        return;
    }

    for (auto& [name, pVulkanPass] : pVulkanScene->passes)
    {
        if (pVulkanPass->currentSize != renderContext.frameBufferSize)
        {
            pVulkanPass->currentSize = renderContext.frameBufferSize;

            // Use render framebuffers for now
            vulkan::framebuffer_create(ctx, pVulkanPass->frameBuffer, renderContext.colorBuffers, &renderContext.depthBuffer, pVulkanPass->renderPass);
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
                // Create it
                pVulkanPass->geometryPipeline = pipeline_create(ctx, g_vertexLayout, pVulkanPass->geometryPipelineLayout, pVulkanPass->renderPass, shaderStages);
                debug_set_pipeline_name(ctx.device, pVulkanPass->geometryPipeline, debug_pass_name(*pVulkanPass, "Pipeline"));
            }
            
            validation_set_shaders({});
        }
    }

    for (auto& [name, pVulkanGeom] : pVulkanScene->geometries)
    {
        model_stage(ctx, pVulkanGeom->model);
    }

    // Validation layer may set an error, meaning this scene is not valid!
    // Destroy it, and reset the error trigger
    if (validation_get_error_state())
    {
        scene_destroy(ctx, scene);
        validation_clear_error_state();
    }
}

void scene_destroy(VulkanContext& ctx, Scene& scene)
{
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    if (!pVulkanScene)
    {
        return;
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
        ctx.device.destroyDescriptorSetLayout(pVulkanPass->descriptorSetLayout);
        ctx.device.freeDescriptorSets(ctx.descriptorPool, pVulkanPass->descriptorSet);
        ctx.device.destroyPipeline(pVulkanPass->geometryPipeline);
        ctx.device.destroyPipelineLayout(pVulkanPass->geometryPipelineLayout);
    }
    pVulkanScene->passes.clear();

    // TODO: Delete scene objects
    ctx.mapVulkanScene.erase(&scene);

    scene.valid = false;
}

void scene_render(VulkanContext& ctx, RenderContext& renderContext, Scene& scene)
{
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    if (!pVulkanScene)
    {
        return;
    }

    // Prepare stuff that might have changed
    scene_prepare(ctx, renderContext, scene);

    if (scene.valid == false)
    {
        return; 
    }

    for (auto& [name, pVulkanPass] : pVulkanScene->passes)
    {
        // Setup the camera for this pass
        // pVulkanPass->pPass->camera.orbitDelta = glm::vec2(4.0f, 0.0f);
        camera_set_film_size(pVulkanPass->pPass->camera, glm::ivec2(renderContext.frameBufferSize.x, renderContext.frameBufferSize.y));
        camera_pre_render(pVulkanPass->pPass->camera);

        glm::vec3 meshPos = glm::vec3(0.0f, 0.0f, 0.0f);
        pVulkanPass->vsUBO.time.x = timer_get_elapsed_seconds(globalTimer);
        pVulkanPass->vsUBO.projection = camera_get_projection(pVulkanPass->pPass->camera);
        pVulkanPass->vsUBO.model = camera_get_lookat(pVulkanPass->pPass->camera);
        utils_copy_to_memory(ctx, pVulkanPass->vsUniform.memory, pVulkanPass->vsUBO);

        // Clear
        vk::ClearValue clearValues[2];
        clearValues[0].color = clear_color(pVulkanPass->pPass->clearColor);
        clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        // Draw geometry
        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = pVulkanPass->renderPass;
        renderPassBeginInfo.framebuffer = pVulkanPass->frameBuffer.framebuffer;
        renderPassBeginInfo.renderArea.extent.width = renderContext.frameBufferSize.x;
        renderPassBeginInfo.renderArea.extent.height = renderContext.frameBufferSize.y;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        auto rect = renderContext.frameBufferSize;
        auto pVulkanPassPtr = pVulkanPass.get();

        try
        {
            utils_with_command_buffer(ctx, [&,pVulkanPassPtr](auto cmd) -> void {
                debug_set_commandbuffer_name(ctx.device, cmd, debug_pass_name(*pVulkanPassPtr, "RenderBuffer"));
                debug_begin_region(cmd, debug_pass_name(*pVulkanPassPtr, "Draw"), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
                cmd.setViewport(0, viewport(glm::uvec2(rect.x, rect.y)));
                cmd.setScissor(0, rect2d(glm::uvec2(rect.x, rect.y)));
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pVulkanPassPtr->geometryPipelineLayout, 0, pVulkanPassPtr->descriptorSet, nullptr);
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
            });
        }
        catch(std::exception& ex)
        {
            validation_error(ex.what());

            scene_destroy(ctx, scene);
            
            ctx.deviceState = DeviceState::Lost;
            return;
        }
    }
}

} // namespace vulkan
