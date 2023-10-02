#include <zest/file/file.h>
#include <zest/logger/logger.h>

#include <lodepng.h>

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

#include "zest/imgui/imgui.h"

namespace vulkan
{
namespace
{
// Vertex layout for this example
VertexLayout g_vertexLayout{ {
    Component::VERTEX_COMPONENT_POSITION,
    Component::VERTEX_COMPONENT_UV,
    Component::VERTEX_COMPONENT_COLOR,
    Component::VERTEX_COMPONENT_NORMAL,
} };

#pragma pack(push, 1) // Ensure correct structure packing
struct BMPHeader {
    uint16_t fileType{0x4D42}; // BM
    uint32_t fileSize{0};       // File size in bytes
    uint16_t reserved1{0};      // Reserved (0)
    uint16_t reserved2{0};      // Reserved (0)
    uint32_t dataOffset{54};    // Offset to image data (bytes)
    uint32_t headerSize{40};    // Header size (bytes)
    int32_t width{0};           // Image width (pixels)
    int32_t height{0};          // Image height (pixels)
    uint16_t planes{1};         // Number of color planes (1)
    uint16_t bitsPerPixel{24};  // Bits per pixel (24 for 8-bit R, G, B components)
    uint32_t compression{0};    // Compression method (0 for no compression)
    uint32_t dataSize{0};       // Size of raw image data (bytes, 0 for no compression)
    int32_t horizontalRes{2835}; // Horizontal resolution (pixels/meter)
    int32_t verticalRes{2835};   // Vertical resolution (pixels/meter)
    uint32_t colors{0};          // Number of colors in the palette (0 for no palette)
    uint32_t importantColors{0}; // Important colors (0 means all are important)
};
#pragma pack(pop) // Restore default structure packing

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
    auto pSwapSurface = main_window_current_swap_image(ctx);

    auto pDefaultTargetSurface = get_default_target(ctx, scene);
    if (pSwapSurface && pDefaultTargetSurface && pDefaultTargetSurface->uploadMemory)
    {
        ctx.device.waitIdle();

        utils_with_command_buffer(ctx, [&](vk::CommandBuffer copyCmd) {
            debug_set_commandbuffer_name(ctx.device, copyCmd, "Buffer::ImageUpload");

            auto sz = pDefaultTargetSurface->pSurface->currentSize;

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

        ctx.device.waitIdle();

        VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(ctx.device, pDefaultTargetSurface->uploadImage, &subResource, &subResourceLayout);

        char* pMem = (char*)ctx.device.mapMemory(pDefaultTargetSurface->uploadMemory, 0, VK_WHOLE_SIZE);
        pMem += subResourceLayout.offset;

        std::ofstream file(path / fmt::format("Frame_{}.bmp", scene.GlobalFrameCount), std::ios::out | std::ios::binary);

        auto width = pDefaultTargetSurface->pSurface->currentSize.x;
        auto height = pDefaultTargetSurface->pSurface->currentSize.y;
        lodepng::encode((path / fmt::format("Frame_{}.bmp", scene.GlobalFrameCount)).string(), (const unsigned char*)pMem, width, height); 

        /*
        // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
        bool colorSwizzle = false;

        // Check if source is BGR
        // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
        if (!pDefaultTargetSurface->isBlitUpload)
        {
            std::vector<vk::Format> formatsBGR = { vk::Format::eB8G8R8A8Srgb, vk::Format::eB8G8R8A8Unorm, vk::Format::eB8G8R8A8Snorm };
            colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), (vk::Format)pDefaultTargetSurface->format) != formatsBGR.end());
        }

        //colorSwizzle = true;
        // ppm binary pixel data
        for (uint32_t y = 0; y < height; y++)
        {
            unsigned int* row = (unsigned int*)(pMem + (subResourceLayout.rowPitch * (height - y - 1)));
            for (uint32_t x = 0; x < width; x++)
            {
                if (colorSwizzle)
                {
                    file.write((char*)row + 2, 1);
                    file.write((char*)row + 1, 1);
                    file.write((char*)row, 1);
                }
                else
                {
                    file.write((char*)row, 3);
                }
                row++;
            }
            //pMem += subResourceLayout.rowPitch;
        }
        file.close();
        */

        std::cout << "Screenshot saved to disk" << std::endl;

        ctx.device.unmapMemory(pDefaultTargetSurface->uploadMemory);
    }
}

} // namespace vulkan
