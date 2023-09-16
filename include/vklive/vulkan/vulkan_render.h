#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_framebuffer.h"
#include "vklive/camera.h"

namespace vulkan
{
struct VulkanModel;

void render_init(VulkanContext& ctx);
void render_destroy(VulkanContext& ctx);
void render(VulkanContext& ctx, const glm::vec4& rect, Scene& scene);
void* render_get_texture_id(VulkanContext& ctx, Scene& scene);

} // namespace vulkan
