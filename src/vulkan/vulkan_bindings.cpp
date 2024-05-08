#include <fmt/format.h>

//#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv-reflect/spirv_reflect.h> 
#include "config_app.h"

#include <zest/logger/logger.h>

#include <vklive/vulkan/vulkan_bindings.h>
#include <vklive/vulkan/vulkan_reflect.h>
#include <vklive/vulkan/vulkan_pass.h>

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

bool bindings_merge(VulkanPass& vulkanPass, const std::vector<BindingSets*>& mergeInputs, BindingSets& bindingSets)
{
    auto& vulkanScene = vulkanPass.vulkanScene;
    for (auto& merge : mergeInputs)
    {
        for (const auto& [set, bindings] : *merge)
        {
            VulkanBindingSet copy = bindings;
            for (auto& [index, value] : copy.bindings)
            {
                if (bindingSets[set].bindings.find(index) != bindingSets[set].bindings.end())
                {
                    if (bindingSets[set].bindings[index].descriptorType != value.descriptorType)
                    {
                        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Pass {}: Bindings for set {}, index {} do not match", vulkanPass.pass.name, set, index), vulkanScene.pScene->sceneGraphPath, vulkanPass.pass.scriptPassLine);
                        bindingSets.clear();
                        return false;
                    }
                }
                bindingSets[set].bindings[index] = value;
            }

            for (auto& [index, value] : copy.bindingMeta)
            {
                // Do we care if meta doesn't match? Should be taken care of above by binding type
                bindingSets[set].bindingMeta[index] = value;
            }

            if (bindingSets[set].bindingMeta.size() != bindingSets[set].bindings.size())
            {
                scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Pass {}: Bindings meta mismatch?", vulkanPass.pass.name), vulkanScene.pScene->sceneGraphPath);
                bindingSets.clear();
                return false;
            }
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

    return true;
}

} // namespace vulkan
