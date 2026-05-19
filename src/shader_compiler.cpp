#include <vklive/shader_compiler.h>

#include <cstdint>
#include <map>
#include <regex>
#include <vector>

#include <spirv-reflect/spirv_reflect.h>

#include <zest/string/string_utils.h>

namespace
{

ShaderBindingType spv_reflect_descriptor_type_to_shader_binding_type(SpvReflectDescriptorType type)
{
    switch (type)
    {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return ShaderBindingType::Sampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return ShaderBindingType::CombinedImageSampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return ShaderBindingType::SampledImage;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return ShaderBindingType::StorageImage;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return ShaderBindingType::UniformTexelBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return ShaderBindingType::StorageTexelBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return ShaderBindingType::UniformBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return ShaderBindingType::StorageBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return ShaderBindingType::UniformBufferDynamic;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return ShaderBindingType::StorageBufferDynamic;
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return ShaderBindingType::InputAttachment;
    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return ShaderBindingType::AccelerationStructure;
    default:
        return ShaderBindingType::Unknown;
    }
}

ShaderStageFlags spv_reflect_shader_stage_to_shader_stage_flags(SpvReflectShaderStageFlagBits stage)
{
    switch (stage)
    {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Vertex);
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Fragment);
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Geometry);
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Compute);
    case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::RayGen);
    case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::ClosestHit);
    case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Miss);
    case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::AnyHit);
    default:
        return 0;
    }
}

} // namespace

// Vulkan errors are not very consistent!
// EX1, HLSL "(9): error at column 2, HLSL parsing failed."
// Here I use several regex to pull out the bits I need.
// But sometimes Vulkan isn't really pointing at the right column; and the text output varies depending on the error.
bool shader_parse_output(const std::string& strOutput, const fs::path& shaderPath, Scene& scene)
{
    bool errors = false;
    if (strOutput.empty())
    {
        return errors;
    }

    auto p = fs::canonical(shaderPath);

    std::map<int32_t, std::vector<Message>> messageLines;

    std::vector<Message> noLineMessages;
    auto error_lines = Zest::string_split(strOutput, "\r\n");
    for (auto& error_line : error_lines)
    {
        Message msg;
        msg.severity = MessageSeverity::Message;

        // TODO: Includes, etc.
        msg.path = p;

        try
        {
            std::regex errorRegex(".*(error:)", std::regex::icase);
            std::regex warningRegex(".*(warning:)", std::regex::icase);
            std::regex lineRegex(":([0-9]+):", std::regex::icase);
            std::regex messageRegex(".*:[0-9]+:(.*)", std::regex::icase);

            std::regex pathRegex(".*(WARNING|ERROR): (.*):[0-9]+:");

            std::smatch match;
            if (std::regex_search(error_line, match, pathRegex) && match.size() > 1)
            {
                msg.path = Zest::string_trim(match[2].str());
            }

            if (std::regex_search(error_line, match, errorRegex) && match.size() > 1)
            {
                msg.severity = MessageSeverity::Error;
                errors = true;
            }
            else if (std::regex_search(error_line, match, warningRegex) && match.size() > 1)
            {
                msg.severity = MessageSeverity::Message;
            }

            if (std::regex_search(error_line, match, messageRegex) && match.size() > 1)
            {
                msg.text = Zest::string_trim(match[1].str());
            }
            else
            {
                msg.text = error_line;
            }

            if (std::regex_search(error_line, match, lineRegex) && match.size() > 1)
            {
                msg.line = std::stoi(match[1].str()) - 1;
            }
            else
            {
                // Don't ignore errors on non line messages
                if (msg.severity == MessageSeverity::Error)
                {
                    msg.line = 0;
                    noLineMessages.push_back(msg);
                }
                // Ignore no line
                else
                {
                    continue;
                }
            }
        }
        catch (...)
        {
            msg.text = "Failed to parse compiler error:\n" + error_line;
            msg.line = -1;
            msg.severity = MessageSeverity::Error;
        }

        messageLines[msg.line].push_back(msg);
    }

    // Combine
    for (auto& [line, msg] : messageLines)
    {
        Message addMessage = msg[0];
        if (msg.size() > 1)
        {
            for (int i = 1; i < msg.size(); i++)
            {
                if (msg[i].severity > addMessage.severity)
                {
                    addMessage.severity = msg[i].severity;
                }
                // Ignore this useless/stupid error continuation
                if (msg[i].text.find("compilation terminated") != std::string::npos)
                {
                    continue;
                }
                addMessage.text.append("\n");
                addMessage.text.append(msg[i].text);
            }
        }
        scene_report_error(scene, addMessage.severity, addMessage.text, addMessage.path, addMessage.line);
    }
    return errors;
}

bool shader_reflect_binding_sets(const void* spirvData, size_t spirvSize, const fs::path& shaderPath, ShaderBindingSets& bindingSets)
{
    bindingSets.clear();

    SpvReflectShaderModule module = {};
    SpvReflectResult result = spvReflectCreateShaderModule(spirvSize, spirvData, &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        spvReflectDestroyShaderModule(&module);
        return false;
    }

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        spvReflectDestroyShaderModule(&module);
        return false;
    }

    for (auto& set : sets)
    {
        for (uint32_t index = 0; index < set->binding_count; ++index)
        {
            auto& bindingReflect = *set->bindings[index];

            ShaderBinding shaderBinding;
            shaderBinding.binding = bindingReflect.binding;
            shaderBinding.type = spv_reflect_descriptor_type_to_shader_binding_type(bindingReflect.descriptor_type);
            shaderBinding.count = 1;
            for (uint32_t dim = 0; dim < bindingReflect.array.dims_count; dim++)
            {
                shaderBinding.count *= bindingReflect.array.dims[dim];
            }
            shaderBinding.stageFlags = spv_reflect_shader_stage_to_shader_stage_flags(module.shader_stage);
            bindingSets[set->set].bindings[shaderBinding.binding] = shaderBinding;

            ShaderBindingMeta meta;
            meta.name = bindingReflect.name;
            meta.shaderPath = shaderPath;
            // SPIR-V reflection does not include source ranges; later diagnostics scan by metadata name.
            meta.line = 0;
            bindingSets[set->set].bindingMeta[shaderBinding.binding] = meta;
        }
    }

    spvReflectDestroyShaderModule(&module);
    return true;
}

bool shader_reflect_binding_sets(const std::vector<uint32_t>& spirv, const fs::path& shaderPath, ShaderBindingSets& bindingSets)
{
    return shader_reflect_binding_sets(spirv.data(), spirv.size() * sizeof(uint32_t), shaderPath, bindingSets);
}
