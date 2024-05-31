#include <regex>
#include <set>

#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <sstream>

//#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv_reflect.h>

#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <zest/logger/logger.h>
#include <zest/string/string_utils.h>

#include "config_app.h"
#include <vklive/process/process.h>
#include <vklive/vulkan/vulkan_reflect.h>
#include <vklive/vulkan/vulkan_shader.h>

namespace vulkan
{

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

bool shader_reflect(const std::string& spirv, VulkanShader& vulkanShader)
{
    SpvReflectShaderModule module = {};
    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.c_str(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

#ifdef _DEBUG
    std::ostringstream str;
    const spv_reflect::ShaderModule mod(spirv.size(), spirv.c_str());
    WriteReflection(mod, false, str);
    LOG_SCOPE(DBG, str.str());
#endif

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    for (auto& set : sets)
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        for (uint32_t index = 0; index < set->binding_count; ++index)
        {
            auto& bindingReflect = *set->bindings[index];

            vk::DescriptorSetLayoutBinding layout_binding;
            layout_binding.binding = bindingReflect.binding;
            layout_binding.descriptorType = static_cast<vk::DescriptorType>(bindingReflect.descriptor_type);
            layout_binding.descriptorCount = 1;
            for (uint32_t dim = 0; dim < bindingReflect.array.dims_count; dim++)
            {
                layout_binding.descriptorCount *= bindingReflect.array.dims[dim];
            }
            layout_binding.stageFlags = static_cast<vk::ShaderStageFlagBits>(module.shader_stage);
            vulkanShader.bindingSets[set->set].bindings[layout_binding.binding] = layout_binding;

            VulkanBindingMeta meta;
            meta.name = bindingReflect.name;
            meta.shaderPath = vulkanShader.pShader->path;
            // TODO: Can we provide the range here?
            // The reflection doesn't give us file offsets, so we would have to scan the file and find the declarations
            meta.line = 0;
            vulkanShader.bindingSets[set->set].bindingMeta[layout_binding.binding] = meta;
        }
    }
    LOG_SCOPE(DBG, "Shader: " << vulkanShader.pShader->path.filename() << ", Bindings:");
    bindings_dump(vulkanShader.bindingSets);
    spvReflectDestroyShaderModule(&module);
    return true;
}

std::shared_ptr<VulkanShader> vulkan_shader_create(VulkanContext& ctx, VulkanScene& vulkanScene, Shader& shader)
{
    std::shared_ptr<VulkanShader> spShader = std::make_shared<VulkanShader>(&shader);

    auto out_path = fs::temp_directory_path() / "vklive";
    fs::create_directories(out_path);
    out_path = out_path / (shader.path.filename().string() + ".spirv");

    if (shader.path.extension().string() == ".vert")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
    }
    else if (shader.path.extension().string() == ".frag")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
    }
    else if (shader.path.extension().string() == ".geom")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eGeometry;
    }
    else if (shader.path.extension().string() == ".rchit")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    }
    else if (shader.path.extension().string() == ".rgen")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eRaygenKHR;
    }
    else if (shader.path.extension().string() == ".rmiss")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eMissKHR;
    }
    else
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Unknown shader type: {}", shader.path.filename().string()));
        return nullptr;
    }

    spShader->bindingSets.clear();
    spShader->shaderCreateInfo.module = nullptr;

    fs::path compiler_path;
    // fs::path cross_path;
#ifdef WIN32
    compiler_path = Zest::runtree_find_path("bin/win/glslangValidator.exe");
#elif defined(__APPLE__)
    compiler_path = Zest::runtree_find_path("bin/mac/glslangValidator");
#elif defined(__linux__)
    compiler_path = Zest::runtree_find_path("bin/linux/glslangValidator");
#endif
    std::vector<std::string> args{
        compiler_path.string(),
        "-V",
        shader.path.string(),
        "-o",
        out_path.string(),
        "-l",
        "-g",
        fmt::format("-I{}", fs::canonical(shader.path.parent_path()).string()),
        fmt::format("-I{}", fs::canonical(Zest::runtree_path() / "shaders/include").string()),
    };

    if (scene_is_raytracer(shader.path))
    {
        args.push_back("--target-env");
        args.push_back("vulkan1.2");
    }

    std::string output;
    auto ret = run_process(
        args,
        &output);
    if (ret)
    {
        LOG(DBG, "Could not run glslangConvertor");
        return nullptr;
    }

    if (shader_parse_output(output, shader.path, *vulkanScene.pScene))
    {
        return nullptr;
    }

    auto spirv = Zest::file_read(out_path);
    if (spirv.empty())
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not get spirv for shader: {}", shader.path.filename().string()), shader.path);
        return nullptr;
    }

    if (!shader_reflect(spirv, *spShader))
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not reflect spirv for shader: {}", shader.path.filename().string()), shader.path);
    }

    // Create the shader modules
    spShader->shaderCreateInfo.module = ctx.device.createShaderModule(
        vk::ShaderModuleCreateInfo({}, spirv.size(), (const uint32_t*)spirv.c_str()));

    debug_set_shadermodule_name(ctx.device,
        spShader->shaderCreateInfo.module,
        std::string("Shader::Module::") + shader.path.filename().string());

    if (spShader->shaderCreateInfo.module)
    {
        vulkanScene.shaderStages[shader.path] = spShader;
        spShader->shaderCreateInfo.pName = "main";
    }
    else
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not create shader: {}", shader.path.filename().string()));
    }

    return spShader;
}

void vulkan_shader_destroy(VulkanContext& ctx, VulkanShader& shader)
{
    if (shader.shaderCreateInfo.module)
    {
        ctx.device.destroyShaderModule(shader.shaderCreateInfo.module);
    }
}

} // namespace vulkan
