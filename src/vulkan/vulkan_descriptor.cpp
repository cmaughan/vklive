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

// Global state
vk::DescriptorPool currentPool{ VK_NULL_HANDLE };
PoolSizes descriptorSizes;
std::vector<vk::DescriptorPool> usedPools;
std::vector<vk::DescriptorPool> freePools;
std::unordered_map<DescriptorLayoutInfo, vk::DescriptorSetLayout, DescriptorLayoutHash> layoutCache;

vk::DescriptorPool create_pool(VulkanContext& ctx, const PoolSizes& poolSizes, int count, vk::DescriptorPoolCreateFlags flags)
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

    return ctx.device.createDescriptorPool(pool_info);
}

vk::DescriptorPool grab_pool(VulkanContext& ctx)
{
    if (freePools.size() > 0)
    {
        vk::DescriptorPool pool = freePools.back();
        freePools.pop_back();
        return pool;
    }
    else
    {
        return create_pool(ctx, descriptorSizes, 1000, vk::DescriptorPoolCreateFlags());
    }
}

} // namespace

void descriptor_reset_pools(VulkanContext& ctx)
{
    for (auto p : usedPools)
    {
        vkResetDescriptorPool(ctx.device, p, 0);
    }

    freePools = usedPools;
    usedPools.clear();
    currentPool = VK_NULL_HANDLE;
}

bool descriptor_allocate(VulkanContext& ctx, vk::DescriptorSet* set, vk::DescriptorSetLayout layout)
{
    if (!currentPool)
    {
        currentPool = grab_pool(ctx);
        usedPools.push_back(currentPool);
    }

    vk::DescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;

    allocInfo.pSetLayouts = &layout;
    allocInfo.descriptorPool = currentPool;
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
        currentPool = grab_pool(ctx);
        usedPools.push_back(currentPool);

        allocResult = ctx.device.allocateDescriptorSets(&allocInfo, set);

        // if it still fails then we have big issues
        if (allocResult == vk::Result::eSuccess)
        {
            return true;
        }
    }

    return false;
}

void descriptor_init(VulkanContext& ctx)
{
}

void descriptor_cleanup(VulkanContext& ctx)
{
    // delete every pool held
    for (auto p : freePools)
    {
        vkDestroyDescriptorPool(ctx.device, p, nullptr);
    }
    for (auto p : usedPools)
    {
        vkDestroyDescriptorPool(ctx.device, p, nullptr);
    }

    // delete every descriptor layout held
    for (auto pair : layoutCache)
    {
        vkDestroyDescriptorSetLayout(ctx.device, pair.second, nullptr);
    }

    layoutCache.clear();
    usedPools.clear();
    freePools.clear();
}

vk::DescriptorSetLayout create_descriptor_layout(VulkanContext& ctx, vk::DescriptorSetLayoutCreateInfo& info)
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

    auto it = layoutCache.find(layoutinfo);
    if (it != layoutCache.end())
    {
        return (*it).second;
    }
    else
    {
        vk::DescriptorSetLayout layout = ctx.device.createDescriptorSetLayout(info);

        // layoutCache.emplace()
        // add to cache
        layoutCache[layoutinfo] = layout;
        return layout;
    }
}

void descriptor_bind_buffer(VulkanContext& ctx, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorBufferInfo* bufferInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags)
{
    vk::DescriptorSetLayoutBinding newBinding{};

    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    builder.bindings.push_back(newBinding);

    vk::WriteDescriptorSet newWrite{};
    newWrite.pNext = nullptr;

    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pBufferInfo = bufferInfo;
    newWrite.dstBinding = binding;

    builder.writes.push_back(newWrite);
}

void descriptor_bind_image(VulkanContext& ctx, DescriptorBuilder& builder, uint32_t binding, vk::DescriptorImageInfo* imageInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags)
{
    vk::DescriptorSetLayoutBinding newBinding{};

    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    builder.bindings.push_back(newBinding);

    vk::WriteDescriptorSet newWrite{};
    newWrite.pNext = nullptr;
    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pImageInfo = imageInfo;
    newWrite.dstBinding = binding;

    builder.writes.push_back(newWrite);
}

void descriptor_reset(VulkanContext& ctx, DescriptorBuilder& builder)
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

bool descriptor_build(VulkanContext& ctx, DescriptorBuilder& builder, const std::string& debugName)
{
    // build layout first
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.pNext = nullptr;
    layoutInfo.pBindings = builder.bindings.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(builder.bindings.size());

    builder.layout = create_descriptor_layout(ctx, layoutInfo);

    // allocate descriptor
    bool success = descriptor_allocate(ctx, &builder.set, builder.layout);
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