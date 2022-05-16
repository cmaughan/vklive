#include <regex>
#include <set>

#include <fmt/format.h>

#include <vklive/file/file.h>
#include <vklive/file/runtree.h>
#include <vklive/vulkan/vulkan_shader.h>
#include <vklive/process/process.h>
#include <vklive/logger/logger.h>
#include "config_app.h"
#include <vklive/string/string_utils.h>

namespace vulkan
{

// Vulkan errors are not very consistent!
// EX1, HLSL "(9): error at column 2, HLSL parsing failed."
// Here I use several regex to pull out the bits I need.
// But sometimes Vulkan isn't really pointing at the right column; and the text output varies depending on the error.
bool shader_parse_output(const std::string& strOutput, const fs::path& shaderPath, std::vector<Message>& messages)
{
    bool errors = false;
    if (strOutput.empty())
    {
        return errors;
    }

    auto p = fs::canonical(shaderPath);

    std::map<int32_t, std::vector<Message>> messageLines;

    std::vector<Message> noLineMessages;
    auto error_lines = string_split(strOutput, "\r\n");
    for (auto& error_line : error_lines)
    {
        Message msg;
        msg.severity = MessageSeverity::Message;

        /*
        // Try to find paths
        auto paths = string_split(error_line, " ");
        for (auto& pathString : paths)
        {
            auto tok = string_trim(pathString, " \n\r:");
            try
            {
                auto p = fs::path(tok);
                if (fs::exists(p))
                {
                    msg.path = fs::canonical(p);
                }
            }
            catch (std::exception& ex)
            {
                LOG(INFO, ex.what());
            }
        }
        */

        // TODO: Includes, etc.
        msg.path = p;

        try
        {
            std::regex errorRegex(".*(error:)", std::regex::icase);
            std::regex warningRegex(".*(warning:)", std::regex::icase);
            std::regex lineRegex(":([0-9]+):", std::regex::icase);
            std::regex messageRegex(".*:[0-9]+:(.*)", std::regex::icase);
            std::smatch match;
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
                msg.text = string_trim(match[1].str());
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
        messages.push_back(addMessage);
    }
    return errors;
}

vk::ShaderModule shader_create(VulkanContext& ctx, const fs::path& strPath, std::vector<Message>& messages)
{
    auto out_path = fs::temp_directory_path() / (strPath.filename().string() + ".spirv");

    fs::path compiler_path;
#ifdef WIN32
    compiler_path = runtree_find_path("bin/win/glslangValidator.exe");
#else
    compiler_path = runtree_find_path("bin/mac/glslangValidator");
#endif
    std::string output;
    auto ret = run_process(
        {   compiler_path.string(),
            "-V",
            "-l",
            "-g",
            fmt::format("-I{}", fs::canonical(runtree_path() / "shaders/include").string()),
            "-o",
            out_path.string(),
            strPath.string()
        }, &output);
    if (ret)
    {
        LOG(DBG, "Could not run glslangConvertor");
        return vk::ShaderModule{};
    }
    LOG(INFO, output);

    if (shader_parse_output(output, strPath, messages))
    {
        return vk::ShaderModule{}; 
    }

    auto spirv = file_read(out_path);

    // Create the shader modules
    auto mod = ctx.device.createShaderModule(vk::ShaderModuleCreateInfo({}, spirv.size(), (const uint32_t*)spirv.c_str()));
    debug_set_shadermodule_name(ctx.device, mod, std::string("Shader::Module::") + strPath.filename().string());
    return mod;
}

} // namespace vulkan
