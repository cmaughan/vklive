#pragma once

#include <vklive/file/file.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/scene.h>

namespace vulkan
{
vk::ShaderModule shader_create(VulkanContext& ctx, const fs::path& strPath, std::vector<Message>& messages);
} // namespace vulkan
