#pragma once

#include <vector>

#include <vklive/shader_bindings.h>
#include <vklive/vulkan/vulkan_context.h>

namespace vulkan
{

struct VulkanPass;

using VulkanBindingMeta = ShaderBindingMeta;
using VulkanBindingSet = ShaderBindingSet;
using BindingSets = ShaderBindingSets;

vk::DescriptorType shader_binding_type_to_vulkan(ShaderBindingType type);
vk::ShaderStageFlags shader_stage_flags_to_vulkan(ShaderStageFlags flags);
vk::DescriptorSetLayoutBinding shader_binding_to_vulkan_layout(const ShaderBinding& binding);

bool bindings_merge(VulkanPass& pass, const std::vector<BindingSets*>& sets, BindingSets& bindingSets);
void bindings_dump(const BindingSets& bindingSets);

} // namespace vulkan
