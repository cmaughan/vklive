#include <fmt/format.h>
#include <spirv_reflect.h>

#include "config_app.h"

#include <vklive/logger/logger.h>
#include <vklive/vulkan/vulkan_bindings.h>
#include <vklive/vulkan/vulkan_reflect.h>

namespace vulkan
{

void bindings_dump(const BindingSets& bindingSets, uint32_t indent)
{
    for (auto& [set, bindings] : bindingSets)
    {
        LOG_INDENT(DBG, indent, "Set: " << set);
        for (auto& [index, binding] : bindings.bindings)
        {
            LOG_INDENT(DBG, indent + 2, fmt::format("{} {} (Count: {}) Flags: {}", index, ToStringDescriptorType((SpvReflectDescriptorType)binding.descriptorType),
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
