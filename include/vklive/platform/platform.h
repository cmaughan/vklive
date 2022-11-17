#pragma once

#include <unordered_map>
#include <filesystem>

// This specialization doesn't exist on linux/mac
#ifndef WIN32
namespace std {
    template <>
    struct hash<fs::path> {
        size_t operator()(const fs::path &path) const {
            return hash_value(path);            }
    };
}
#endif
