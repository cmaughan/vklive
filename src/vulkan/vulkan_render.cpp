#include <zest/file/file.h>
#include <zest/logger/logger.h>

#include <lodepng.h>
       
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include <stb_image_write.h>

#include <fstream>

#include "config_app.h"
#include "vklive/vulkan/vulkan_command.h"
#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_pipeline.h"
#include "vklive/vulkan/vulkan_render.h"
#include "vklive/vulkan/vulkan_scene.h"
#include "vklive/vulkan/vulkan_shader.h"
#include "vklive/vulkan/vulkan_uniform.h"
#include "vklive/vulkan/vulkan_utils.h"

#include <zest/thread/threadpool.h>
#include <zest/time/profiler.h>
#include "zest/imgui/imgui.h"

namespace vulkan
{
namespace
{

TPool threadPool;

// Vertex layout for this example
VertexLayout g_vertexLayout{ {
    Component::VERTEX_COMPONENT_POSITION,
    Component::VERTEX_COMPONENT_UV,
    Component::VERTEX_COMPONENT_COLOR,
    Component::VERTEX_COMPONENT_NORMAL,
} };

} // namespace

void render_init(VulkanContext& ctx)
{
}

void render_destroy(VulkanContext& ctx)
{
}

void render_check_framebuffer(VulkanContext& ctx, const glm::uvec2& size)
{
    if (ctx.frameBufferSize == size)
    {
        return;
    }

    LOG(DBG, "Framebuffer Resize: " << size.x << ", " << size.y);
    ctx.frameBufferSize = size;
}

void render(VulkanContext& ctx, const glm::vec4& rect, Scene& scene)
{
    // Check the framebuffer
    render_check_framebuffer(ctx, glm::uvec2(rect.z, rect.w));

    auto pVulkanScene = vulkan::vulkan_scene_get(ctx, scene);
    if (pVulkanScene)
    {
        // Render the scene
        vulkan::vulkan_scene_render(ctx, *pVulkanScene);
    }
}

VulkanSurface* get_default_target(VulkanContext& ctx, Scene& scene)
{
    if (ctx.deviceState == DeviceState::Normal)
    {
        // If we have a final target, and we rendered to it
        auto pVulkanScene = vulkan_scene_get(ctx, scene);
        if (pVulkanScene)
        {
            if (pVulkanScene->defaultTarget)
            {
                // Find the thing we just rendered to
                auto itrTargetData = pVulkanScene->surfaces.find(pVulkanScene->defaultTarget);
                if (itrTargetData != pVulkanScene->surfaces.end())
                {
                    return itrTargetData->second.get();
                }
            }
        }
    }
    return nullptr;
}

RenderOutput render_get_output(VulkanContext& ctx, Scene& scene)
{
    RenderOutput out;

    auto pVulkanSurface = get_default_target(ctx, scene);
    if (pVulkanSurface)
    {
        out.pSurface = pVulkanSurface->pSurface;
        if (pVulkanSurface->ImGuiDescriptorSet)
        {
            out.textureId = (ImTextureID)pVulkanSurface->ImGuiDescriptorSet;
        }
    }

    return out;
}

void render_write_output(VulkanContext& ctx, Scene& scene, const fs::path& path)
{
    PROFILE_SCOPE(render_write_output);

    auto pSwapSurface = main_window_current_swap_image(ctx);

    auto pDefaultTargetSurface = get_default_target(ctx, scene);
    if (pSwapSurface && pDefaultTargetSurface && pDefaultTargetSurface->uploadMemory)
    {
        ctx.device.waitIdle();

        glm::uvec2 origin = glm::uvec2(scene.targetViewport.x, scene.targetViewport.y);
        auto sz = glm::uvec2(scene.targetViewport.z - scene.targetViewport.x, scene.targetViewport.w - scene.targetViewport.y);
        //auto sz = pDefaultTargetSurface->pSurface->currentSize;
        {
            PROFILE_SCOPE(image_upload);
            utils_with_command_buffer(ctx, [&](vk::CommandBuffer copyCmd) {
                debug_set_commandbuffer_name(ctx.device, copyCmd, "Buffer::ImageUpload");


                surface_set_layout(ctx, copyCmd, pSwapSurface->image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
                surface_set_layout(ctx, copyCmd, pDefaultTargetSurface->uploadImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                if (pDefaultTargetSurface->isBlitUpload)
                {
                    vk::ImageBlit blitRegion(
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                        { vk::Offset3D(scene.targetViewport.x, scene.targetViewport.y, 0), vk::Offset3D(sz.x, sz.y, 1) },
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                        { vk::Offset3D(0, 0, 0), vk::Offset3D(sz.x, sz.y, 1) });

                    copyCmd.blitImage(pSwapSurface->image, vk::ImageLayout::eTransferSrcOptimal, pDefaultTargetSurface->uploadImage, vk::ImageLayout::eTransferDstOptimal, blitRegion, vk::Filter::eNearest);
                }
                else
                {
                    vk::ImageCopy copyRegion(
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                        vk::Offset3D(scene.targetViewport.x, scene.targetViewport.y, 0),
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                        vk::Offset3D(0, 0, 0),
                        { sz.x, sz.y, 1 });
                    copyCmd.copyImage(pSwapSurface->image, vk::ImageLayout::eTransferSrcOptimal, pDefaultTargetSurface->uploadImage, vk::ImageLayout::eTransferDstOptimal, copyRegion);
                }
                surface_set_layout(ctx, copyCmd, pSwapSurface->image, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
            });
        }

        ctx.device.waitIdle();

        VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(ctx.device, pDefaultTargetSurface->uploadImage, &subResource, &subResourceLayout);

        char* pMem = (char*)ctx.device.mapMemory(pDefaultTargetSurface->uploadMemory, 0, VK_WHOLE_SIZE);
        pMem += subResourceLayout.offset;

        // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
        bool colorSwizzle = true;

        // Check if source is BGR
        // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
        if (!pDefaultTargetSurface->isBlitUpload)
        {
            std::vector<vk::Format> formatsBGR = { vk::Format::eB8G8R8A8Srgb, vk::Format::eB8G8R8A8Unorm, vk::Format::eB8G8R8A8Snorm };
            colorSwizzle = !(std::find(formatsBGR.begin(), formatsBGR.end(), (vk::Format)pDefaultTargetSurface->format) != formatsBGR.end());
        }

        auto pSrc = (char*)malloc(subResourceLayout.rowPitch * sz.y);
        memcpy(pSrc, pMem, subResourceLayout.rowPitch * sz.y);
            
        ctx.device.unmapMemory(pDefaultTargetSurface->uploadMemory);

        threadPool.enqueue([=]() {
            PROFILE_SCOPE(write_png_thread)
            std::vector<char> image;
            image.resize(sz.x * sz.y * 3);

            // ppm binary pixel data
            uint32_t index = 0;
            for (uint32_t y = 0; y < sz.y; y++)
            {
                uint32_t* row = (uint32_t*)(pSrc + (subResourceLayout.rowPitch * y));
                for (uint32_t x = 0; x < sz.x; x++)
                {
                    if (colorSwizzle)
                    {
                        image[index++] = *((char*)row + 2);
                        image[index++] = *((char*)row + 1);
                        image[index++] = *((char*)row);
                    }
                    else
                    {
                        image[index++] = *((char*)row);
                        image[index++] = *((char*)row + 1);
                        image[index++] = *((char*)row + 2);
                    }
                    row++;
                }
            }
            
            auto fileName = path / fmt::format("Frame_{:05}.png", scene.GlobalFrameCount);
            lodepng::encode(fileName.string(), (const unsigned char*)image.data(), sz.x, sz.y, LCT_RGB);

            free(pSrc);
        });
    }
}

} // namespace vulkan
