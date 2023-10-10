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

struct NVGcontext;

class Scene;
struct Pass;

struct PythonModule
{
    std::shared_ptr<pkpy::VM> spVM;
    fs::path path;
    std::string script;
    std::string errors;
    std::shared_ptr<pkpy::CodeObject> spCode;
};

std::shared_ptr<PythonModule> python_compile(Scene& scene, const fs::path& path);
bool python_run_2d(PythonModule& mod, IDevice* pDevice, Scene& scene, ImDrawList* pDrawList, const glm::vec4& viewport);
void python_run_pass(NVGcontext* vg, Pass& pass, const glm::uvec2& targetSize);


