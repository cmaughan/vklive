#include "vklive/vulkan/vulkan_render.h"
#include "config_app.h"
#include "vklive/file/file.h"
#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_pipeline.h"
#include "vklive/vulkan/vulkan_shader.h"
#include "vklive/vulkan/vulkan_uniform.h"
#include "vklive/vulkan/vulkan_utils.h"
#include "vklive/vulkan/vulkan_scene.h"

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
std::shared_ptr<RenderContext> render_context(VulkanContext& ctx)
{
    return std::static_pointer_cast<RenderContext>(ctx.spRenderData);
}

void render_init(VulkanContext& ctx)
{
    auto spRender = std::make_shared<RenderContext>();
    ctx.spRenderData = spRender;
}

void render_destroy_images(VulkanContext& ctx, RenderContext& renderContext)
{
    for (auto& buffer : renderContext.colorBuffers)
    {
        image_destroy(ctx, buffer);
    }
    renderContext.colorBuffers.clear();

    if (renderContext.depthBuffer.format != vk::Format::eUndefined)
    {
        image_destroy(ctx, renderContext.depthBuffer);
    }
}

void render_destroy(VulkanContext& ctx)
{
    auto spRender = render_context(ctx);
    render_destroy_images(ctx, *spRender);

    ctx.spRenderData = nullptr;
}

void render_create_images(VulkanContext& ctx, RenderContext& renderContext, const glm::uvec2& size, vk::Format colorFormat, vk::Format depthFormat)
{
    render_destroy_images(ctx, renderContext);

    vk::ImageUsageFlags colorUsage = vk::ImageUsageFlagBits::eSampled;
    vk::ImageUsageFlags depthUsage = vk::ImageUsageFlags();

    // Color attachment
    vk::ImageCreateInfo image;
    image.imageType = vk::ImageType::e2D;
    image.extent.width = size.x;
    image.extent.height = size.y;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = vk::SampleCountFlagBits::e1;
    image.tiling = vk::ImageTiling::eOptimal;
    image.usage = vk::ImageUsageFlagBits::eColorAttachment | colorUsage;
    image.format = colorFormat;
    renderContext.colorBuffers.push_back(image_create(ctx, image, vk::MemoryPropertyFlagBits::eDeviceLocal));
    debug_set_image_name(ctx.device, renderContext.colorBuffers[0].image, "Render:DefaultColor");
    debug_set_devicememory_name(ctx.device, renderContext.colorBuffers[0].memory, "Render:DefaultColorMemory");

    vk::ImageViewCreateInfo colorImageView;
    colorImageView.viewType = vk::ImageViewType::e2D;
    colorImageView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.format = colorFormat;
    colorImageView.image = renderContext.colorBuffers[0].image;
    renderContext.colorBuffers[0].view = ctx.device.createImageView(colorImageView);
    debug_set_imageview_name(ctx.device, renderContext.colorBuffers[0].view, "Render:DefaultColorView");

    bool useDepth = depthFormat != vk::Format::eUndefined;
    // Depth stencil attachment
    if (useDepth)
    {
        image.format = depthFormat;
        image.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | depthUsage;
        renderContext.depthBuffer = image_create(ctx, image, vk::MemoryPropertyFlagBits::eDeviceLocal);
        debug_set_image_name(ctx.device, renderContext.depthBuffer.image, "Render:DefaultDepth");
        debug_set_devicememory_name(ctx.device, renderContext.depthBuffer.memory, "Render:DefaultDepthMemory");

        vk::ImageViewCreateInfo depthStencilView;
        depthStencilView.viewType = vk::ImageViewType::e2D;
        depthStencilView.format = depthFormat;
        depthStencilView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        depthStencilView.subresourceRange.levelCount = 1;
        depthStencilView.subresourceRange.layerCount = 1;
        depthStencilView.image = renderContext.depthBuffer.image;
        renderContext.depthBuffer.view = ctx.device.createImageView(depthStencilView);
        debug_set_imageview_name(ctx.device, renderContext.depthBuffer.view, "Render:DepthBufferView");
    }
}

void render_check_framebuffer(VulkanContext& ctx, const glm::uvec2& size)
{
    auto spRender = render_context(ctx);
    if (spRender->frameBufferSize == size)
    {
        return;
    }

    // Might still be rendering to/with this FB, so wait for it.
    ctx.device.waitIdle();

    // Destroy old
    spRender->frameBufferSize = size;

    render_create_images(ctx, *spRender, glm::uvec2(size), vk::Format::eR8G8B8A8Unorm, vk::Format::eD32Sfloat);

    image_set_sampling(ctx, spRender->colorBuffers[0]);
}

void render(VulkanContext& ctx, const glm::vec4& rect, Scene& scene)
{
    auto spRender = render_context(ctx);

    // Check the framebuffer
    render_check_framebuffer(ctx, glm::uvec2(rect.z, rect.w));

    // Render the scene
    vulkan::scene_render(ctx, *spRender, scene);
}

} // namespace vulkan
