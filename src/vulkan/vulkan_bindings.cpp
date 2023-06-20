#include <fmt/format.h>

#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv_reflect.h>

#include "config_app.h"

#include <zest/logger/logger.h>

#include <vklive/vulkan/vulkan_bindings.h>
#include <vklive/vulkan/vulkan_reflect.h>

namespace vulkan
{

void bindings_dump(const BindingSets& bindingSets)
{
    LOG_SCOPE(DBG, "Bindings:");
    for (auto& [set, bindings] : bindingSets)
    {
        LOG_SCOPE(DBG, "Set: " << set);
        for (auto& [index, binding] : bindings.bindings)
        {
            LOG(DBG, fmt::format("{} {} (Count: {}) Flags: {}", index, ToStringDescriptorType((SpvReflectDescriptorType)binding.descriptorType),
                binding.descriptorCount, to_string(binding.stageFlags)));
        }
    }
}

BindingSets bindings_merge(const std::vector<BindingSets*>& mergeInputs)
{
    std::map<uint32_t, VulkanBindingSet> bindingSets;
    for (auto& merge : mergeInputs)
    {
        for (const auto& [set, bindings] : *merge)
        {
            VulkanBindingSet copy = bindings;
            bindingSets[set].bindings.merge(copy.bindings);
            bindingSets[set].bindingMeta.merge(copy.bindingMeta);
        }
    }

    // Merge the stage flags for matching bindings
    // TODO: Is this all we need to do here?
    for (auto& [setA, bindingSetA] : bindingSets)
    {
        auto& bindingsA = bindingSetA.bindings;
        for (auto& merge : mergeInputs)
        {
            auto& mergeSet = *merge;
            for (auto& bindingsB : mergeSet[setA].bindings)
            {
                auto itr = bindingsA.find(bindingsB.first);
                if (itr != bindingsA.end())
                {
                    itr->second.stageFlags |= bindingsB.second.stageFlags;
                }
            }
        }
    }

    return bindingSets;
}

} // namespace vulkan
