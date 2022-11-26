#pragma once

#include <array>
#include <unordered_map>
#include <vector>
#include <vklive/vulkan/vulkan_context.h>

namespace vulkan
{

struct DescriptorLayoutInfo
{
    // good idea to turn this into a inlined array
    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    bool operator==(const DescriptorLayoutInfo& other) const
    {
        if (other.bindings.size() != bindings.size())
        {
            return false;
        }
        else
        {
            // compare each of the bindings is the same. Bindings are sorted so they will match
            for (int i = 0; i < bindings.size(); i++)
            {
                if (other.bindings[i].binding != bindings[i].binding)
                {
                    return false;
                }
                if (other.bindings[i].descriptorType != bindings[i].descriptorType)
                {
                    return false;
                }
                if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
                {
                    return false;
                }
                if (other.bindings[i].stageFlags != bindings[i].stageFlags)
                {
                    return false;
                }
            }
            return true;
        }
    }
    size_t hash() const
    {
        using std::hash;
        using std::size_t;

        size_t result = hash<size_t>()(bindings.size());

        for (const vk::DescriptorSetLayoutBinding& binding : bindings)
        {
            auto b = (VkDescriptorSetLayoutBinding)binding;
            // pack the binding data into a single int64. Not fully correct but its ok
            size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

            // shuffle the packed binding data and xor it with the main hash
            result ^= hash<size_t>()(binding_hash);
        }

        return result;
    }
};

struct DescriptorLayoutHash
{
    std::size_t operator()(const DescriptorLayoutInfo& k) const
    {
        return k.hash();
    }
};

struct DescriptorCache
{
    vk::DescriptorPool currentPool{ VK_NULL_HANDLE };
    std::vector<vk::DescriptorPool> usedPools;
    std::vector<vk::DescriptorPool> freePools;
    std::unordered_map<DescriptorLayoutInfo, vk::DescriptorSetLayout, DescriptorLayoutHash> layoutCache;
};

struct DescriptorBuilder
{
    std::vector<vk::WriteDescriptorSet> writes;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    vk::DescriptorSet set;
    vk::DescriptorSetLayout layout;
    std::vector<vk::DescriptorImageInfo> imageInfos;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
};

void descriptor_reset_pools(VulkanContext& ctx, DescriptorCache& cache);
vk::DescriptorSetLayout descriptor_create_layout(VulkanContext& ctx, DescriptorCache& cache, vk::DescriptorSetLayoutCreateInfo& info);
bool descriptor_allocate(VulkanContext& ctx, DescriptorCache& cache, vk::DescriptorSet* set, vk::DescriptorSetLayout layout);
void vulkan_descriptor_destroy_pools(VulkanContext& ctx, DescriptorCache& cache);

void descriptor_bind_buffer(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorBufferInfo* bufferInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags);
void descriptor_bind_image(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorImageInfo* imageInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags);
bool descriptor_build(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder, const std::string& debugName = "");
void descriptor_reset(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder);

DescriptorCache& descriptor_get_cache(VulkanContext& ctx);

} // namespace vulkan
