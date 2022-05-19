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

    renderContext.colorBuffers.resize(1);
    image_create(ctx, renderContext.colorBuffers[0], size, colorFormat, true, "RenderDefault");

    bool useDepth = depthFormat != vk::Format::eUndefined;
    if (useDepth)
    {
        image_create_depth(ctx, renderContext.depthBuffer, size, depthFormat, false, "RenderDefault");
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
    vulkan::vulkan_scene_render(ctx, *spRender, scene);
}

} // namespace vulkan
