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

void render_destroy(VulkanContext& ctx)
{
    auto spRender = render_context(ctx);
    ctx.spRenderData = nullptr;
}

} // namespace vulkan
