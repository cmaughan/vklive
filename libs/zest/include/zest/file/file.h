#pragma once

#include <functional>

#include <filesystem>
namespace fs = std::filesystem;

namespace Zest
{

std::string file_read(const fs::path& fileName);
bool file_write(const fs::path& fileName, const void* pData, size_t size);
bool file_write(const fs::path& fileName, const std::string& str);
fs::path file_get_relative_path(fs::path from, fs::path to);

fs::path file_exe_path();
fs::path file_documents_path();
fs::path file_roaming_path();
fs::path file_appdata_path();

fs::path file_init_settings(const std::string& appName, const fs::path& defaultSettings, const fs::path& targetPath, bool forceReset = false);

void file_folder_copy(const fs::path& source, const fs::path& target, bool recursive = true);

std::vector<fs::path> file_gather_folders(const fs::path& root);

std::vector<fs::path> file_gather_files(const fs::path& root, bool recursive = true);

} // namespace Zest
