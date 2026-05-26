#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace vklive_nvim
{

struct NvimProjectFiles
{
    std::filesystem::path project_root;
    std::vector<std::filesystem::path> files;
};

struct NvimHostOptions
{
    std::filesystem::path project_root;
    std::string executable = "nvim";
    int columns = 80;
    int rows = 24;
};

std::vector<std::string> build_open_project_tab_commands(const NvimProjectFiles& project);

} // namespace vklive_nvim
