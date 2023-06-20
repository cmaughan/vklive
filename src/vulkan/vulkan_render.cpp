#include <zest/file/file.h>
#include <zest/logger/logger.h>

#include "vklive/vulkan/vulkan_render.h"
#include "config_app.h"
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

} // namespace vulkan
