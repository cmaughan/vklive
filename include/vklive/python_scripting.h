#pragma once

#include <filesystem>
namespace fs = std::filesystem;

class Pass;
namespace pkpy
{
class VM;
}

struct PythonModule
{
    pkpy::VM* pVM = nullptr;
    fs::path path;
    std::string script;
    std::string errors;
};

//void python_init();
//void python_tick(ImDrawList* pDrawList, const glm::vec4& viewport);
//void python_destroy();

std::shared_ptr<PythonModule> python_compile(const fs::path& path);
void python_run(PythonModule& mod);


