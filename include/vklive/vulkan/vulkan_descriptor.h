#pragma once

#include <array>
#include <unordered_map>
#include <vector>
#include <vklive/vulkan/vulkan_context.h>

namespace vulkan
{

struct DescriptorBuilder 
{
    std::vector<vk::WriteDescriptorSet> writes;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    vk::DescriptorSet set;
    vk::DescriptorSetLayout layout;
};

void descriptor_reset_pools(VulkanContext& ctx);
bool descriptor_allocate(VulkanContext& ctx, vk::DescriptorSet* set, vk::DescriptorSetLayout layout);
void descriptor_init(VulkanContext& ctx);
void descriptor_cleanup(VulkanContext& ctx);

void descriptor_bind_buffer(VulkanContext& ctx, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorBufferInfo* bufferInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags);
void descriptor_bind_image(VulkanContext& ctx, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorImageInfo* imageInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags);
bool descriptor_build(VulkanContext& ctx, DescriptorBuilder& builder, const std::string& debugName = "");
void descriptor_reset(VulkanContext& ctx, DescriptorBuilder& builder);

} // namespace vulkan
