#pragma once

#include <vklive/file/file.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/scene.h>

namespace vulkan
{

struct VulkanBindingMeta
{
    std::string name;
    fs::path shaderPath;
    int32_t line = -1;
    std::pair<int32_t, int32_t> range = std::make_pair(-1, -1);
};

struct VulkanBindingSet
{
    // [Index, Binding]
    std::map<uint32_t, vk::DescriptorSetLayoutBinding> bindings;
    std::map<uint32_t, VulkanBindingMeta> bindingMeta;
    
    vk::DescriptorSet descriptorSet;
    vk::DescriptorSetLayout descriptorLayout;
};

struct VulkanShader
{
    VulkanShader(Shader* pS)
        : pShader(pS)
    {
    }
    Shader* pShader;

    // [Set, [index, Binding]]
    std::map<uint32_t, VulkanBindingSet> bindingSets;
    vk::PipelineShaderStageCreateInfo shaderCreateInfo;
};

std::shared_ptr<VulkanShader> shader_create(VulkanContext& ctx, Scene& scene, Shader& shader);

} // namespace vulkan
