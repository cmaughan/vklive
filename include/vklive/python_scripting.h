#pragma once

#include <filesystem>
#include <zest/math/imgui_glm.h>

namespace fs = std::filesystem;

struct ImDrawList;
struct IDevice;

namespace pkpy
{
class VM;
struct CodeObject;

}

class Scene;

struct PythonModule
{
    std::shared_ptr<pkpy::VM> spVM;
    fs::path path;
    std::string script;
    std::string errors;
    std::shared_ptr<pkpy::CodeObject> spCode;
};

//void python_init();
//void python_tick(ImDrawList* pDrawList, const glm::vec4& viewport);
//void python_destroy();

std::shared_ptr<PythonModule> python_compile(Scene& scene, const fs::path& path);
bool python_run_2d(PythonModule& mod, IDevice* pDevice, Scene& scene, ImDrawList* pDrawList, const glm::vec4& viewport);


