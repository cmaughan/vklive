#pragma once

#include <zest/file/file.h>

#include <vklive/scene.h>
#include <vklive/vulkan/vulkan_context.h>

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
};

using BindingSets = std::map<uint32_t, VulkanBindingSet>;

bool bindings_merge(VulkanPass& pass, const std::vector<BindingSets*>& sets, BindingSets& bindingSets);
void bindings_dump(const BindingSets& bindingSets);

} // namespace vulkan
