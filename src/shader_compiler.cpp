#include <vklive/shader_compiler.h>

#include <cstdint>
#include <map>
#include <regex>
#include <vector>

#include <zest/string/string_utils.h>

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
