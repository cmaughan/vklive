#pragma once

#include <zest/file/file.h>

#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_bindings.h>
#include <vklive/scene.h>

namespace vulkan
{

struct VulkanShader
{
    VulkanShader(Shader* pS)
        : pShader(pS)
    {
    }
    Shader* pShader;

    // [Set, [index, Binding]]
    BindingSets bindingSets;
    vk::PipelineShaderStageCreateInfo shaderCreateInfo;
};

std::shared_ptr<VulkanShader> vulkan_shader_create(VulkanContext& ctx, VulkanScene& scene, Shader& shader);
void vulkan_shader_destroy(VulkanContext& ctx, VulkanShader& shader);
bool vulkan_shader_format(const fs::path& path);

} // namespace vulkan
