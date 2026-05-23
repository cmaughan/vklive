#pragma once

#include "zest/file/file.h"

namespace Zest
{

void runtree_init(const char* pszAppRoot, const char* pszBuildPath);
void runtree_destroy();
fs::path runtree_find_path(const fs::path& p);
std::string runtree_load_asset(const fs::path& p);
fs::path runtree_path();

}
