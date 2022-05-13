#pragma once

#include <vector>

std::error_code run_process(const std::vector<std::string>& args, std::string* pOutput);
