#pragma once

#include <filesystem>

namespace fs = std::filesystem;

void nodegraph_seed_default_theme();
bool nodegraph_load_theme_file(const fs::path& path);
bool nodegraph_save_theme_file(const fs::path& path);
