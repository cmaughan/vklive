#include <filesystem>

#include "vklive/vulkan/vulkan_command.h"
#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_imgui.h"
#include "vklive/vulkan/vulkan_render.h"
#include "vklive/vulkan/vulkan_utils.h"

#include "vklive/file/runtree.h"

#include "vklive/imgui/imgui_sdl.h"

#include "config_app.h"

#include <vklive/file/file.h>
#include <vklive/file/runtree.h>
#include <vklive/process/process.h>
#include <vklive/vulkan/vulkan_shader.h>

static std::string iniPath;

namespace vulkan
{
std::shared_ptr<ImGuiContext> imgui_context(VulkanContext& ctx)
{
    if (!ctx.spImGuiData)
    {
        ctx.spImGuiData = std::make_shared<ImGuiContext>();
    }
    return std::static_pointer_cast<ImGuiContext>(ctx.spImGuiData);
}

void imgui_create_shaders(VulkanContext& ctx)
{
    auto imgui = imgui_context(ctx);

    VulkanShader data(nullptr);

    Scene scene(runtree_path() / "shaders");
    Shader vertShader(runtree_find_path("shaders/imgui.vert"));
    Shader fragShader(runtree_find_path("shaders/imgui.frag"));

    // Create the shader modules
    if (!imgui->shaderModuleVert)
    {
        auto spShader = shader_create(ctx, scene, vertShader);
        imgui->shaderModuleVert = spShader->shaderCreateInfo.module;
    }
    if (!imgui->shaderModuleFrag)
    {
        auto spShader = shader_create(ctx, scene, fragShader);
        imgui->shaderModuleFrag = spShader->shaderCreateInfo.module;
    }
}

bool create_device_objects(VulkanContext& ctx)
{
    auto imgui = imgui_context(ctx);
    if (!imgui->fontSampler)
    {
        imgui->fontSampler = ctx.device.createSampler(vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear));
        debug_set_sampler_name(ctx.device, imgui->fontSampler, "ImGui::FontSampler");
    }

    if (!imgui->descriptorSetLayout)
    {
        auto binding = vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, &imgui->fontSampler);
        imgui->descriptorSetLayout = ctx.device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), binding));
        debug_set_descriptorsetlayout_name(ctx.device, imgui->descriptorSetLayout, "ImGui::DescriptorSetLayout");
    }

    if (!imgui->pipelineLayout)
    {
        // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
        std::vector<vk::PushConstantRange> push_constants = { vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, sizeof(float) * 0, sizeof(float) * 4) };
        imgui->pipelineLayout = ctx.device.createPipelineLayout(vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), imgui->descriptorSetLayout, push_constants));
        debug_set_pipelinelayout_name(ctx.device, imgui->pipelineLayout, "ImGui::PipelineLayout");
    }

    imgui_create_shaders(ctx);

    // Pipeline
    vk::PipelineShaderStageCreateInfo stage[2];
    stage[0].stage = vk::ShaderStageFlagBits::eVertex;
    stage[0].module = imgui->shaderModuleVert;
    stage[0].pName = "main";
    stage[1].stage = vk::ShaderStageFlagBits::eFragment;
    stage[1].module = imgui->shaderModuleFrag;
    stage[1].pName = "main";

    vk::VertexInputBindingDescription binding_desc[1];
    binding_desc[0].stride = sizeof(ImDrawVert);
    binding_desc[0].inputRate = vk::VertexInputRate::eVertex;

    vk::VertexInputAttributeDescription attribute_desc[3];
    attribute_desc[0].location = 0;
    attribute_desc[0].binding = binding_desc[0].binding;
    attribute_desc[0].format = vk::Format::eR32G32Sfloat;
    attribute_desc[0].offset = IM_OFFSETOF(ImDrawVert, pos);
    attribute_desc[1].location = 1;
    attribute_desc[1].binding = binding_desc[0].binding;
    attribute_desc[1].format = vk::Format::eR32G32Sfloat;
    attribute_desc[1].offset = IM_OFFSETOF(ImDrawVert, uv);
    attribute_desc[2].location = 2;
    attribute_desc[2].binding = binding_desc[0].binding;
    attribute_desc[2].format = vk::Format::eR8G8B8A8Unorm;
    attribute_desc[2].offset = IM_OFFSETOF(ImDrawVert, col);

    vk::PipelineVertexInputStateCreateInfo vertex_info;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = 3;
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    vk::PipelineInputAssemblyStateCreateInfo ia_info;
    ia_info.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewport_info;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo raster_info;
    raster_info.polygonMode = vk::PolygonMode::eFill;
    raster_info.cullMode = vk::CullModeFlagBits::eNone;
    raster_info.frontFace = vk::FrontFace::eCounterClockwise;
    raster_info.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo ms_info;
    ms_info.rasterizationSamples = ctx.MSAASamples;

    vk::PipelineColorBlendAttachmentState color_attachment[1];
    color_attachment[0].blendEnable = 1;
    color_attachment[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    color_attachment[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    color_attachment[0].colorBlendOp = vk::BlendOp::eAdd;
    color_attachment[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
    color_attachment[0].dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    color_attachment[0].alphaBlendOp = vk::BlendOp::eAdd;
    color_attachment[0].colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineDepthStencilStateCreateInfo depth_info;
    vk::PipelineColorBlendStateCreateInfo blend_info;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    vk::DynamicState dynamic_states[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.dynamicStateCount = (uint32_t)IM_ARRAYSIZE(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    vk::GraphicsPipelineCreateInfo info;
    info.flags = vk::PipelineCreateFlags();
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &ia_info;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = imgui->pipelineLayout;
    info.renderPass = imgui->renderPass;
    info.subpass = imgui->subpass;
    imgui->pipeline = ctx.device.createGraphicsPipelines(ctx.pipelineCache, info).value[0];
    debug_set_pipeline_name(ctx.device, imgui->pipeline, "ImGui::Pipeline");
    return true;
}

void imgui_destroy_device_objects(VulkanContext& ctx)
{
    auto imgui = imgui_context(ctx);

    imgui_viewport_destroy_all(ctx);
    imgui_destroy_font_upload_objects(ctx);

    ctx.device.destroyShaderModule(imgui->shaderModuleFrag);
    ctx.device.destroyShaderModule(imgui->shaderModuleVert);
    ctx.device.destroyImageView(imgui->fontView);
    ctx.device.destroyImage(imgui->fontImage);
    ctx.device.freeDescriptorSets(ctx.descriptorPool, imgui->fontDescriptorSet);
    ctx.device.freeMemory(imgui->fontMemory);
    ctx.device.destroySampler(imgui->fontSampler);
    ctx.device.destroyDescriptorSetLayout(imgui->descriptorSetLayout);
    ctx.device.destroyPipelineLayout(imgui->pipelineLayout);
    ctx.device.destroyPipeline(imgui->pipeline);
    imgui->shaderModuleFrag = nullptr;
    imgui->shaderModuleVert = nullptr;
    imgui->fontView = nullptr;
    imgui->fontImage = nullptr;
    imgui->fontMemory = nullptr;
    imgui->fontSampler = nullptr;
    imgui->descriptorSetLayout = nullptr;
    imgui->pipelineLayout = nullptr;
    imgui->pipeline = nullptr;
    imgui->renderPass = nullptr; /* destroyed in the window */
}

void imgui_shutdown(VulkanContext& ctx)
{
    auto imgui = imgui_context(ctx);
    ImGuiIO& io = ImGui::GetIO();

    // First destroy objects in all viewports
    imgui_destroy_device_objects(ctx);

    // Manually delete main viewport render data in-case we haven't initialized for viewports
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    if (ImGuiViewportData* vd = (ImGuiViewportData*)main_viewport->RendererUserData)
        IM_DELETE(vd);
    main_viewport->RendererUserData = NULL;

    // Clean up windows
    ImGui::DestroyPlatformWindows();

    io.BackendRendererName = NULL;
    io.BackendRendererUserData = NULL;
}

// Register a texture
// FIXME: This is experimental in the sense that we are unsure how to best design/tackle this problem, please post to https://github.com/ocornut/imgui/pull/914 if you have suggestions.
vk::DescriptorSet imgui_add_texture(VulkanContext& ctx, vk::Sampler sampler, vk::ImageView image_view, vk::ImageLayout image_layout, vk::DescriptorSetLayout descriptorLayout)
{
    // Create Descriptor Set:
    auto descriptor_set = ctx.device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(ctx.descriptorPool, descriptorLayout));

    // Update the Descriptor Set:
    {
        vk::DescriptorImageInfo desc_image;
        desc_image.sampler = sampler;
        desc_image.imageView = image_view;
        desc_image.imageLayout = image_layout;

        vk::WriteDescriptorSet write_desc;
        write_desc.dstSet = (vk::DescriptorSet)descriptor_set[0];
        write_desc.descriptorCount = 1;
        write_desc.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_desc.setImageInfo(desc_image);
        ctx.device.updateDescriptorSets(write_desc, {});
    }
    return descriptor_set[0];
}

void imgui_upload_font(VulkanContext& ctx)
{
    auto imgui = imgui_context(ctx);
    ImGuiIO& io = ImGui::GetIO();

    // Note: Adjust font size as appropriate!
    auto fontPath = runtree_find_path("fonts/Cousine-Regular.ttf");
    io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16 * ctx.vdpi);

    // Upload Fonts
    {
        // Use any command queue
        vk::CommandPool command_pool = ctx.mainWindowData.frames[ctx.mainWindowData.frameIndex].commandPool;
        vk::CommandBuffer command_buffer = ctx.mainWindowData.frames[ctx.mainWindowData.frameIndex].commandBuffer;

        ctx.device.resetCommandPool(command_pool, vk::CommandPoolResetFlags());

        command_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        debug_begin_region(command_buffer, "ImGui:Upload Font", glm::vec4(1.0f));

        // Create the texture
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        size_t upload_size = width * height * 4 * sizeof(char);

        // Create the Image:
        {
            imgui->fontImage = ctx.device.createImage(vk::ImageCreateInfo(vk::ImageCreateFlags(),
                vk::ImageType::e2D,
                vk::Format::eR8G8B8A8Unorm,
                vk::Extent3D(width, height, 1),
                1, // mip levels
                1, // array layers
                vk::SampleCountFlagBits::e1,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                vk::SharingMode::eExclusive,
                0,
                nullptr,
                vk::ImageLayout::eUndefined));

            auto req = ctx.device.getImageMemoryRequirements(imgui->fontImage);
            auto memoryType = utils_memory_type(ctx, vk::MemoryPropertyFlagBits::eDeviceLocal, req.memoryTypeBits);
            imgui->fontMemory = ctx.device.allocateMemory(vk::MemoryAllocateInfo(req.size, memoryType));

            ctx.device.bindImageMemory(imgui->fontImage, imgui->fontMemory, 0);

            debug_set_image_name(ctx.device, imgui->fontImage, "ImGui::Font");
            debug_set_devicememory_name(ctx.device, imgui->fontMemory, "ImGui::FontMemory");
        }

        // Create the Image View:
        imgui->fontView = ctx.device.createImageView(vk::ImageViewCreateInfo(vk::ImageViewCreateFlags(),
            imgui->fontImage,
            vk::ImageViewType::e2D,
            vk::Format::eR8G8B8A8Unorm,
            vk::ComponentMapping(vk::ComponentSwizzle::eIdentity),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
        debug_set_imageview_name(ctx.device, imgui->fontView, "ImGui::FontView");

        // Create the Descriptor Set:
        imgui->fontDescriptorSet = imgui_add_texture(ctx, imgui->fontSampler, imgui->fontView, vk::ImageLayout::eShaderReadOnlyOptimal, imgui->descriptorSetLayout);
        debug_set_descriptorset_name(ctx.device, imgui->fontDescriptorSet, "ImGui::FontDescriptorSet");

        // Create the Upload Buffer:
        imgui->uploadBuffer = ctx.device.createBuffer(vk::BufferCreateInfo(vk::BufferCreateFlags(), upload_size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive));
        debug_set_buffer_name(ctx.device, imgui->uploadBuffer, "ImGui::UploadBuffer");

        // Calculate memory alignment
        auto req = ctx.device.getBufferMemoryRequirements(imgui->uploadBuffer);
        ctx.BufferMemoryAlignment = (ctx.BufferMemoryAlignment > req.alignment) ? ctx.BufferMemoryAlignment : req.alignment;

        // Create the upload buffer memory
        imgui->uploadBufferMemory = ctx.device.allocateMemory(vk::MemoryAllocateInfo(req.size, utils_memory_type(ctx, vk::MemoryPropertyFlagBits::eHostVisible, req.memoryTypeBits)));
        debug_set_devicememory_name(ctx.device, imgui->uploadBufferMemory, "ImGui::UploadBufferMemory");

        // Bind the buffer to the memory
        ctx.device.bindBufferMemory(imgui->uploadBuffer, imgui->uploadBufferMemory, 0);

        // Upload to Buffer:
        {
            auto map = ctx.device.mapMemory(imgui->uploadBufferMemory, 0, upload_size, vk::MemoryMapFlags());
            memcpy(map, pixels, upload_size);
            vk::MappedMemoryRange range;
            range.sType = vk::StructureType::eMappedMemoryRange;
            range.memory = imgui->uploadBufferMemory;
            range.size = upload_size;
            ctx.device.flushMappedMemoryRanges(range);
            ctx.device.unmapMemory(imgui->uploadBufferMemory);
        }

        // Copy to Image:
        {
            vk::ImageMemoryBarrier copy_barrier;
            copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
            copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
            copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.image = imgui->fontImage;
            copy_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            copy_barrier.subresourceRange.levelCount = 1;
            copy_barrier.subresourceRange.layerCount = 1;
            command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, copy_barrier);

            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = width;
            region.imageExtent.height = height;
            region.imageExtent.depth = 1;
            vkCmdCopyBufferToImage(command_buffer, imgui->uploadBuffer, imgui->fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier use_barrier[1] = {};
            use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier[0].image = imgui->fontImage;
            use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            use_barrier[0].subresourceRange.levelCount = 1;
            use_barrier[0].subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
        }

        // Store our identifier
        io.Fonts->SetTexID((ImTextureID)imgui->fontDescriptorSet);

        debug_end_region(command_buffer);
        command_buffer.end();

        command_submit_wait(ctx, ctx.queue, command_buffer);

        imgui_destroy_font_upload_objects(ctx);
    }
}

bool imgui_init(VulkanContext& ctx, const std::string& iniPath, bool viewports)
{
    auto imgui = imgui_context(ctx);
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.IniFilename = iniPath.c_str();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    if (viewports)
    {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform ctx.windows
    }
    // io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak ctx.windowRounding/ctx.windowBg so platform ctx.windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_SDL2_InitForVulkan(ctx.window);

    // Setup backend capabilities flags
    io.BackendRendererUserData = (void*)&ctx;
    io.BackendRendererName = "imgui_impl_vulkan";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)

    imgui->renderPass = ctx.mainWindowData.renderPass;
    imgui->subpass = 0; // info->subpass;

    create_device_objects(ctx);

    // Our render function expect RendererUserData to be storing the window render buffer we need (for the main viewport we won't use ->Window)
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->RendererUserData = IM_NEW(ImGuiViewportData)();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        platform_io.Renderer_CreateWindow = imgui_viewport_create;
        platform_io.Renderer_DestroyWindow = imgui_viewport_destroy;
        platform_io.Renderer_SetWindowSize = imgui_viewport_set_size;
        platform_io.Renderer_RenderWindow = imgui_viewport_render;
        platform_io.Renderer_SwapBuffers = imgui_viewport_swap_buffers;
    }

    imgui_upload_font(ctx);

    return true;
}

// Render function
void imgui_render_drawdata(VulkanContext& ctx, ImDrawData* draw_data, vk::CommandBuffer command_buffer, vk::Pipeline pipeline)
{
    auto imgui = imgui_context(ctx);
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    if (!pipeline)
    {
        pipeline = imgui->pipeline;
    }

    // Allocate array to store enough vertex/index buffers. Each unique viewport gets its own storage.
    ImGuiViewportData* viewport_renderer_data = (ImGuiViewportData*)draw_data->OwnerViewport->RendererUserData;
    IM_ASSERT(viewport_renderer_data != NULL);
    WindowRenderBuffers* wrb = &viewport_renderer_data->renderBuffers;
    if (wrb->frameRenderBuffers == NULL)
    {
        wrb->index = 0;
        wrb->count = ctx.minImageCount;
        wrb->frameRenderBuffers = (FrameRenderBuffers*)IM_ALLOC(sizeof(FrameRenderBuffers) * wrb->count);
        memset(wrb->frameRenderBuffers, 0, sizeof(FrameRenderBuffers) * wrb->count);
    }
    IM_ASSERT(wrb->count == ctx.minImageCount);
    wrb->index = (wrb->index + 1) % wrb->count;
    FrameRenderBuffers* rb = &wrb->frameRenderBuffers[wrb->index];

    if (draw_data->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        if (!rb->vertexBuffer || rb->vertexBufferSize < vertex_size)
        {
            buffer_create_or_resize(ctx, rb->vertexBuffer, rb->vertexBufferMemory, rb->vertexBufferSize, vertex_size, vk::BufferUsageFlagBits::eVertexBuffer);
            debug_set_buffer_name(ctx.device, rb->vertexBuffer, "ImGui::RenderVB");
            debug_set_devicememory_name(ctx.device, rb->vertexBufferMemory, "ImGui::RenderVBMemory");
        }
        if (!rb->indexBuffer || rb->indexBufferSize < index_size)
        {
            buffer_create_or_resize(ctx, rb->indexBuffer, rb->indexBufferMemory, rb->indexBufferSize, index_size, vk::BufferUsageFlagBits::eIndexBuffer);
            debug_set_buffer_name(ctx.device, rb->indexBuffer, "ImGui::RenderIB");
            debug_set_devicememory_name(ctx.device, rb->indexBufferMemory, "ImGui::RenderIBMemory");
        }

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = NULL;
        ImDrawIdx* idx_dst = NULL;
        vtx_dst = (ImDrawVert*)ctx.device.mapMemory(rb->vertexBufferMemory, 0, rb->vertexBufferSize, vk::MemoryMapFlags());
        idx_dst = (ImDrawIdx*)ctx.device.mapMemory(rb->indexBufferMemory, 0, rb->indexBufferSize, vk::MemoryMapFlags());
        for (int n = 0; n < draw_data->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }
        ctx.device.flushMappedMemoryRanges({ vk::MappedMemoryRange(rb->vertexBufferMemory, 0, VK_WHOLE_SIZE), vk::MappedMemoryRange(rb->indexBufferMemory, 0, VK_WHOLE_SIZE) });
        ctx.device.unmapMemory(rb->vertexBufferMemory);
        ctx.device.unmapMemory(rb->indexBufferMemory);
    }

    // Setup desired Vulkan state
    imgui_setup_renderstate(ctx, draw_data, pipeline, command_buffer, rb, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    imgui_setup_renderstate(ctx, draw_data, pipeline, command_buffer, rb, fb_width, fb_height);
                }
                else
                {
                    pcmd->UserCallback(cmd_list, pcmd);
                }
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f)
                {
                    clip_min.x = 0.0f;
                }
                if (clip_min.y < 0.0f)
                {
                    clip_min.y = 0.0f;
                }
                if (clip_max.x > fb_width)
                {
                    clip_max.x = (float)fb_width;
                }
                if (clip_max.y > fb_height)
                {
                    clip_max.y = (float)fb_height;
                }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                command_buffer.setScissor(0, vk::Rect2D({ (int32_t)clip_min.x, (int32_t)clip_min.y }, { (uint32_t)(clip_max.x - clip_min.x), (uint32_t)(clip_max.y - clip_min.y) }));

                // Bind DescriptorSet with font or user texture
                vk::DescriptorSet desc_set[1] = { (VkDescriptorSet)pcmd->TextureId };
                if (sizeof(ImTextureID) < sizeof(ImU64))
                {
                    // We don't support texture switches if ImTextureID hasn't been redefined to be 64-bit. Do a flaky check that other textures haven't been used.
                    IM_ASSERT(pcmd->TextureId == (ImTextureID)imgui->fontDescriptorSet);
                    desc_set[0] = imgui->fontDescriptorSet;
                }
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, imgui->pipelineLayout, 0, 1, desc_set, 0, NULL);

                // Draw
                vkCmdDrawIndexed(command_buffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been called.
    // Our last values will leak into user/application rendering IF:
    // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR dynamic state
    // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself to explicitely set that state.
    // If you use VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before rendering.
    // In theory we should aim to backup/restore those values but I am not sure this is possible.
    // We perform a call to vkCmdSetScissor() to set back a full viewport which is likely to fix things for 99% users but technically this is not perfect. (See github #4644)
    vk::Rect2D scissor = { { 0, 0 }, { (uint32_t)fb_width, (uint32_t)fb_height } };
    command_buffer.setScissor(0, scissor);
}

void imgui_setup_renderstate(VulkanContext& ctx, ImDrawData* draw_data, vk::Pipeline pipeline, vk::CommandBuffer command_buffer, FrameRenderBuffers* rb, int fb_width, int fb_height)
{
    auto imgui = imgui_context(ctx);
    // Bind pipeline:
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    // Bind Vertex And Index Buffer:
    if (draw_data->TotalVtxCount > 0)
    {
        command_buffer.bindVertexBuffers(0, rb->vertexBuffer, vk::DeviceSize(0));
        command_buffer.bindIndexBuffer(rb->indexBuffer, 0, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
    }

    // Setup viewport:
    command_buffer.setViewport(0, vk::Viewport(0, 0, fb_width, fb_height, 0.0f, 1.0f));

    // Setup scale and translation:
    // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        float scale[2];
        scale[0] = 2.0f / draw_data->DisplaySize.x;
        scale[1] = 2.0f / draw_data->DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
        translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];
        vkCmdPushConstants(command_buffer, imgui->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
        vkCmdPushConstants(command_buffer, imgui->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
    }
}

void imgui_render(VulkanContext& ctx, VulkanWindow* wd, ImDrawData* draw_data)
{
    auto imgui = imgui_context(ctx);
    VkResult err;

    // TODO: Main window background
    wd->clearValue = vulkan::clear_color(glm::vec4(0.45f, 0.55f, 0.60f, 1.00f));

    vk::Semaphore image_acquired_semaphore = wd->frameSemaphores[wd->semaphoreIndex].imageAcquiredSemaphore;
    vk::Semaphore render_complete_semaphore = wd->frameSemaphores[wd->semaphoreIndex].renderCompleteSemaphore;
    auto ret = ctx.device.acquireNextImageKHR(wd->swapchain, UINT64_MAX, image_acquired_semaphore);
    if (ret.result == vk::Result::eErrorOutOfDateKHR || ret.result == vk::Result::eSuboptimalKHR)
    {
        ctx.swapChainRebuild = true;
        return;
    }
    wd->frameIndex = ret.value;

    VulkanSwapFrame* fd = &wd->frames[wd->frameIndex];
    auto res = ctx.device.waitForFences(1, &fd->fence, VK_TRUE, UINT64_MAX); // wait indefinitely instead of periodically checking
    res = ctx.device.resetFences(1, &fd->fence);

    ctx.device.resetCommandPool(fd->commandPool, {});
    fd->commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    debug_begin_region(fd->commandBuffer, "ImGui:Render", glm::vec4(1.0f));
    fd->commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(wd->renderPass, fd->framebuffer, vk::Rect2D({ 0, 0 }, { uint32_t(wd->width), uint32_t(wd->height) }), wd->clearValue), vk::SubpassContents::eInline);

    // Record dear imgui primitives into command buffer
    imgui_render_drawdata(ctx, draw_data, fd->commandBuffer);

    try
    {
        // Submit command buffer
        vkCmdEndRenderPass(fd->commandBuffer);
        {
            auto flags = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            auto info = vk::SubmitInfo(image_acquired_semaphore, flags, fd->commandBuffer, render_complete_semaphore);

            debug_end_region(fd->commandBuffer);
            fd->commandBuffer.end();

            ctx.queue.submit(info, fd->fence);
        }
    }
    catch(std::exception& ex)
    {
    }
}

void imgui_destroy_font_upload_objects(VulkanContext& ctx)
{
    auto imgui = imgui_context(ctx);
    ctx.device.destroyBuffer(imgui->uploadBuffer);
    ctx.device.freeMemory(imgui->uploadBufferMemory);

    imgui->uploadBuffer = nullptr;
    imgui->uploadBufferMemory = nullptr;
}

void imgui_render_3d(VulkanContext& ctx, Scene& scene, bool background)
{
    auto imgui = imgui_context(ctx);

    ImVec2 canvas_size;
    ImVec2 canvas_pos;
    ImDrawList* pDrawList = nullptr;

    if (background)
    {
        pDrawList = ImGui::GetBackgroundDrawList();
    }
    else
    {
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(820, 50), ImGuiCond_FirstUseEver);
        ImGui::Begin("Render");
        pDrawList = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos(); // ImDrawList API uses screen coordinates!
        ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // Resize canvas to what's available
        canvas_size.x = std::max(canvas_size.x, 1.0f);
        canvas_size.y = std::max(canvas_size.y, 1.0f);
        ImGui::InvisibleButton("##dummy", canvas_size);
    }

    auto minRect = pDrawList->GetClipRectMin();
    auto maxRect = pDrawList->GetClipRectMax();
    canvas_pos = minRect;
    canvas_size = ImVec2(maxRect.x - minRect.x, maxRect.y - minRect.y);

    if (scene.valid)
    {
        vulkan::render(ctx, glm::vec4(canvas_pos.x, canvas_pos.y, canvas_size.x, canvas_size.y), scene);

        if (ctx.deviceState == DeviceState::Normal)
        {
            auto spRender = render_context(ctx);
            if (spRender->colorBuffers[0].rendered)
            {
                pDrawList->AddImage((ImTextureID)spRender->colorBuffers[0].samplerDescriptorSet,
                    ImVec2(canvas_pos.x, canvas_pos.y),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y));
            }
            else
            {
                pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y), 0xFFFFFFFF, "No passes draw to the this buffer...");
                /*pDrawList->AddRectFilled(
                    ImVec2(canvas_pos.x, canvas_pos.y),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),0xFF0000FF, 10.0f);
                    */
            }
        }
    }

    if (!background)
    {
        ImGui::End();
    }
}

// ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_pos.x, ImGui::GetIO().MousePos.y - canvas_pos.y);
} // namespace vulkan
