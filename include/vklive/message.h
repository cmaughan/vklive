#pragma once

#include <string>
#include <zest/file/file.h>

enum class MessageSeverity
{
    Message,
    Warning,
    Error,
};

struct Message
{
    MessageSeverity severity = MessageSeverity::Message;
    fs::path path;
    std::string text;
    int32_t line = -1;
    std::pair<int32_t, int32_t> range = std::make_pair(- 1, -1);
};
