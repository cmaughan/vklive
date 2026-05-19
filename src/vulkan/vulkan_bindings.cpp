#include <fmt/format.h>

#include "config_app.h"

#include <zest/logger/logger.h>

#include <vklive/vulkan/vulkan_bindings.h>
#include <vklive/vulkan/vulkan_pass.h>

namespace vulkan
{

vk::DescriptorType shader_binding_type_to_vulkan(ShaderBindingType type)
{
    switch (type)
    {
    case ShaderBindingType::Sampler:
        return vk::DescriptorType::eSampler;
    case ShaderBindingType::CombinedImageSampler:
        return vk::DescriptorType::eCombinedImageSampler;
    case ShaderBindingType::SampledImage:
        return vk::DescriptorType::eSampledImage;
    case ShaderBindingType::StorageImage:
        return vk::DescriptorType::eStorageImage;
    case ShaderBindingType::UniformTexelBuffer:
        return vk::DescriptorType::eUniformTexelBuffer;
    case ShaderBindingType::StorageTexelBuffer:
        return vk::DescriptorType::eStorageTexelBuffer;
    case ShaderBindingType::UniformBuffer:
        return vk::DescriptorType::eUniformBuffer;
    case ShaderBindingType::StorageBuffer:
        return vk::DescriptorType::eStorageBuffer;
    case ShaderBindingType::UniformBufferDynamic:
        return vk::DescriptorType::eUniformBufferDynamic;
    case ShaderBindingType::StorageBufferDynamic:
        return vk::DescriptorType::eStorageBufferDynamic;
    case ShaderBindingType::InputAttachment:
        return vk::DescriptorType::eInputAttachment;
    case ShaderBindingType::AccelerationStructure:
        return vk::DescriptorType::eAccelerationStructureKHR;
    case ShaderBindingType::Unknown:
    default:
        return vk::DescriptorType::eSampler;
    }
}

vk::ShaderStageFlags shader_stage_flags_to_vulkan(ShaderStageFlags flags)
{
    vk::ShaderStageFlags vulkanFlags{};
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::Vertex))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eVertex;
    }
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::Fragment))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eFragment;
    }
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::Geometry))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eGeometry;
    }
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::Compute))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eCompute;
    }
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::RayGen))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eRaygenKHR;
    }
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::ClosestHit))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eClosestHitKHR;
    }
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::Miss))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eMissKHR;
    }
    if (flags & static_cast<ShaderStageFlags>(ShaderStageBits::AnyHit))
    {
        vulkanFlags |= vk::ShaderStageFlagBits::eAnyHitKHR;
    }
    return vulkanFlags;
}

vk::DescriptorSetLayoutBinding shader_binding_to_vulkan_layout(const ShaderBinding& binding)
{
    vk::DescriptorSetLayoutBinding layoutBinding;
    layoutBinding.binding = binding.binding;
    layoutBinding.descriptorType = shader_binding_type_to_vulkan(binding.type);
    layoutBinding.descriptorCount = binding.count;
    layoutBinding.stageFlags = shader_stage_flags_to_vulkan(binding.stageFlags);
    return layoutBinding;
}

void bindings_dump(const BindingSets& bindingSets)
{
    LOG_SCOPE(DBG, "Bindings:");
    for (auto& [set, bindings] : bindingSets)
    {
        LOG_SCOPE(DBG, "Set: " << set);
        for (auto& [index, binding] : bindings.bindings)
        {
            LOG(DBG, fmt::format("{} {} (Count: {}) Flags: {}", index, shader_binding_type_to_string(binding.type), binding.count, to_string(shader_stage_flags_to_vulkan(binding.stageFlags))));
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
                    if (bindingSets[set].bindings[index].type != value.type)
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
