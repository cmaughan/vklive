#include <zest/file/file.h>
#include <zest/logger/logger.h>

#include "config_app.h"
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
    auto pVulkanSurface = get_default_target(ctx, scene);
    if (pVulkanSurface)
    {
        //auto pMem = ctx.device.mapMemory(pVulkanSurface->memory, 0, VK_WHOLE_SIZE);


        //ctx.device.unmapMemory(pVulkanSurface->memory);
        /*
        auto result = buffer_create(ctx,
            vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, pVulkanSurface->allocSize);
        debug_set_buffer_name(ctx.device, result.buffer, "FileCopyImage");

        utils_with_command_buffer(ctx, [&](vk::CommandBuffer copyCmd) {
            debug_set_commandbuffer_name(ctx.device, copyCmd, "Buffer::ReadColor");
            copyCmd.copyBuffer(pVulkanSurface->image, result.buffer, vk::BufferCopy(0, 0, pVulkanSurface->allocSize));
        });
        */
 
        //vulkan_buffer_destroy(ctx, result);
    }

}
/*// Transition back the swap chain image after the blit is done
        vks::tools::insertImageMemoryBarrier(
            copyCmd,
            srcImage,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_MEMORY_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        vulkanDevice->flushCommandBuffer(copyCmd, queue);

        // Get layout of the image (including row pitch)
        VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

        // Map image memory so we can start copying from it
        const char* data;
        vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
        data += subResourceLayout.offset;

        std::ofstream file(filename, std::ios::out | std::ios::binary);

        // ppm header
        file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";

        // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
        bool colorSwizzle = false;
        // Check if source is BGR
        // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
        if (!supportsBlit)
        {
            std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
            colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChain.colorFormat) != formatsBGR.end());
        }

        // ppm binary pixel data
        for (uint32_t y = 0; y < height; y++)
        {
            unsigned int *row = (unsigned int*)data;
            for (uint32_t x = 0; x < width; x++)
            {
                if (colorSwizzle)
                {
                    file.write((char*)row+2, 1);
                    file.write((char*)row+1, 1);
                    file.write((char*)row, 1);
                }
                else
                {
                    file.write((char*)row, 3);
                }
                row++;
            }
            data += subResourceLayout.rowPitch;
        }
        file.close();

        std::cout << "Screenshot saved to disk" << std::endl;

        // Clean up resources
        vkUnmapMemory(device, dstImageMemory);
        vkFreeMemory(device, dstImageMemory, nullptr);
        vkDestroyImage(device, dstImage, nullptr);

        screenshotSaved = true;
        */

} // namespace vulkan
