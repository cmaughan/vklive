#pragma once

#include <vklive/file/file.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/scene.h>

namespace vulkan
{

std::shared_ptr<VulkanShader> shader_create(VulkanContext& ctx, SceneGraph& scene, Shader& shader);

} // namespace vulkan
