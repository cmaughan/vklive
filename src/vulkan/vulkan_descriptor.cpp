#include <algorithm>
#include <vklive/vulkan/vulkan_descriptor.h>

namespace vulkan
{

namespace
{

struct PoolSizes
{
    std::vector<std::pair<vk::DescriptorType, float>> sizes = {

        { vk::DescriptorType::eSampler, 4.0f },
        { vk::DescriptorType::eSampledImage, 4.0f },
        { vk::DescriptorType::eCombinedImageSampler, 4.0f },
        { vk::DescriptorType::eStorageImage, 4.0f },
        { vk::DescriptorType::eUniformTexelBuffer, 4.0f },
        { vk::DescriptorType::eStorageTexelBuffer, 4.0f },
        { vk::DescriptorType::eUniformBuffer, 4.0f },
        { vk::DescriptorType::eStorageImage, 4.0f },
        { vk::DescriptorType::eStorageBuffer, 4.0f },
        { vk::DescriptorType::eUniformBufferDynamic, 4.0f },
        { vk::DescriptorType::eStorageBufferDynamic, 4.0f },
        { vk::DescriptorType::eInputAttachment, 4.0f }
    };
};

PoolSizes poolSizes;

// Global state
vk::DescriptorPool create_pool(VulkanContext& ctx, DescriptorCache& cache, const PoolSizes& poolSizes, int count, vk::DescriptorPoolCreateFlags flags)
{
    std::vector<vk::DescriptorPoolSize> sizes;
    sizes.reserve(poolSizes.sizes.size());
    for (auto sz : poolSizes.sizes)
    {
        sizes.push_back({ sz.first, uint32_t(sz.second * count) });
    }
    vk::DescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
    pool_info.flags = flags;
    pool_info.maxSets = count;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();

    auto pool = ctx.device.createDescriptorPool(pool_info);
    debug_set_descriptorpool_name(ctx.device, pool, "Scene::DescriptorPool");
    return pool;
}

vk::DescriptorPool grab_pool(VulkanContext& ctx, DescriptorCache& cache)
{
    if (cache.freePools.size() > 0)
    {
        vk::DescriptorPool pool = cache.freePools.back();
        cache.freePools.pop_back();
        return pool;
    }
    else
    {
        return create_pool(ctx, cache, poolSizes, 1000, vk::DescriptorPoolCreateFlags());
    }
}

} // namespace

void descriptor_reset_pools(VulkanContext& ctx, DescriptorCache& cache)
{
    for (auto p : cache.usedPools)
    {
        vkResetDescriptorPool(ctx.device, p, 0);
    }

    cache.freePools = cache.usedPools;
    cache.usedPools.clear();
    cache.currentPool = VK_NULL_HANDLE;
}

bool descriptor_allocate(VulkanContext& ctx, DescriptorCache& cache, vk::DescriptorSet* set, vk::DescriptorSetLayout layout)
{
    if (!cache.currentPool)
    {
        cache.currentPool = grab_pool(ctx, cache);
        cache.usedPools.push_back(cache.currentPool);
    }

    vk::DescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;

    allocInfo.pSetLayouts = &layout;
    allocInfo.descriptorPool = cache.currentPool;
    allocInfo.descriptorSetCount = 1;

    vk::Result allocResult = ctx.device.allocateDescriptorSets(&allocInfo, set);
    bool needReallocate = false;

    switch (allocResult)
    {
    case vk::Result::eSuccess:
        // all good, return
        return true;

        break;
    case vk::Result::eErrorFragmentedPool:
    case vk::Result::eErrorOutOfPoolMemory:
        // reallocate pool
        needReallocate = true;
        break;
    default:
        // unrecoverable error
        return false;
    }

    if (needReallocate)
    {
        // allocate a new pool and retry
        cache.currentPool = grab_pool(ctx, cache);
        cache.usedPools.push_back(cache.currentPool);

        allocResult = ctx.device.allocateDescriptorSets(&allocInfo, set);

        // if it still fails then we have big issues
        if (allocResult == vk::Result::eSuccess)
        {
            return true;
        }
    }

    return false;
}

void vulkan_descriptor_init(VulkanContext& ctx, DescriptorCache& cache)
{
}

void vulkan_descriptor_cleanup(VulkanContext& ctx, DescriptorCache& cache)
{
    // delete every pool held
    for (auto p : cache.freePools)
    {
        vkDestroyDescriptorPool(ctx.device, p, nullptr);
    }
    for (auto p : cache.usedPools)
    {
        vkDestroyDescriptorPool(ctx.device, p, nullptr);
    }

    // delete every descriptor layout held
    for (auto pair : cache.layoutCache)
    {
        vkDestroyDescriptorSetLayout(ctx.device, pair.second, nullptr);
    }

    cache.layoutCache.clear();
    cache.usedPools.clear();
    cache.freePools.clear();
}

vk::DescriptorSetLayout descriptor_create_layout(VulkanContext& ctx, DescriptorCache& cache, vk::DescriptorSetLayoutCreateInfo& info)
{
    DescriptorLayoutInfo layoutinfo;
    layoutinfo.bindings.reserve(info.bindingCount);
    bool isSorted = true;
    int32_t lastBinding = -1;
    for (uint32_t i = 0; i < info.bindingCount; i++)
    {
        layoutinfo.bindings.push_back(info.pBindings[i]);

        // check that the bindings are in strict increasing order
        if (static_cast<int32_t>(info.pBindings[i].binding) > lastBinding)
        {
            lastBinding = info.pBindings[i].binding;
        }
        else
        {
            isSorted = false;
        }
    }
    if (!isSorted)
    {
        std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(), [](vk::DescriptorSetLayoutBinding& a, vk::DescriptorSetLayoutBinding& b) {
            return a.binding < b.binding;
        });
    }

    auto it = cache.layoutCache.find(layoutinfo);
    if (it != cache.layoutCache.end())
    {
        return (*it).second;
    }
    else
    {
        vk::DescriptorSetLayout layout = ctx.device.createDescriptorSetLayout(info);

        // layoutCache.emplace()
        // add to cache
        cache.layoutCache[layoutinfo] = layout;
        return layout;
    }
}

void descriptor_bind_buffer(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorBufferInfo* bufferInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags)
{
    vk::DescriptorSetLayoutBinding newBinding{};

    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    builder.bindings.push_back(newBinding);

    builder.bufferInfos.push_back(*bufferInfo);

    vk::WriteDescriptorSet newWrite{};
    newWrite.pNext = nullptr;

    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pBufferInfo = &builder.bufferInfos.back();
    newWrite.dstBinding = binding;

    builder.writes.push_back(newWrite);
}

void descriptor_bind_image(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorImageInfo* imageInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags)
{
    vk::DescriptorSetLayoutBinding newBinding{};

    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    builder.bindings.push_back(newBinding);

    builder.imageInfos.push_back(*imageInfo);

    vk::WriteDescriptorSet newWrite{};
    newWrite.pNext = nullptr;
    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pImageInfo = &builder.imageInfos.back();
    newWrite.dstBinding = binding;
    newWrite.dstSet = nullptr;
    newWrite.dstBinding = 0;
    newWrite.dstArrayElement = 0;
    newWrite.pBufferInfo = nullptr;
    newWrite.pTexelBufferView = nullptr;

    builder.writes.push_back(newWrite);
}

void descriptor_reset(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder)
{
    builder.bindings.clear();
    if (builder.layout)
    {
        ctx.device.destroyDescriptorSetLayout(builder.layout);
    }
    builder.writes.clear();
    builder.layout = nullptr;
    builder.set = nullptr;
}

bool descriptor_build(VulkanContext& ctx, DescriptorCache& cache, DescriptorBuilder& builder, const std::string& debugName)
{
    // build layout first
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.pNext = nullptr;
    layoutInfo.pBindings = builder.bindings.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(builder.bindings.size());

    builder.layout = descriptor_create_layout(ctx, cache, layoutInfo);

    // allocate descriptor
    bool success = descriptor_allocate(ctx, cache, &builder.set, builder.layout);
    if (!success)
    {
        return false;
    };

    // write descriptor

    for (vk::WriteDescriptorSet& w : builder.writes)
    {
        w.dstSet = builder.set;
    }

    ctx.device.updateDescriptorSets(static_cast<uint32_t>(builder.writes.size()), builder.writes.data(), 0, nullptr);

    debug_set_descriptorsetlayout_name(ctx.device, builder.layout, debugName);
    debug_set_descriptorset_name(ctx.device, builder.set, debugName);

    return true;
}

} // namespace vulkan