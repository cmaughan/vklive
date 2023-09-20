#include <pocketpy/pocketpy.h>

#include <vklive/python_scripting.h>
#include <zest/logger/logger.h>
#include <zest/file/file.h>

#include <glm/glm.hpp>

#include <vklive/scene.h>

#include <imgui.h>

//using namespace pkpy;
using namespace Zest;

namespace
{
//pkpy::VM* vm = nullptr;
ImDrawList* g_pDrawList = nullptr;
glm::vec4 g_viewPort = glm::vec4(0.0f);
}

VM* make_vm()
{
    auto vm = new pkpy::VM();

    vm->bind(vm->_main, "circle(x: float, y: float, radius: float, thickness: float)", [](VM* vm, ArgsView args) {
        ImVec2 pos(CAST(float, args[0]), CAST(float, args[1]));
        float rad = CAST(float, args[2]);
        float thickness = CAST(float, args[3]);

        pos.x += g_viewPort.x;
        pos.y += g_viewPort.y;

        g_pDrawList->AddCircle(pos, rad, 0xFFFFFFFF, 0, thickness);
        return vm->None;
    });
    return vm;
}

void python_tick(ImDrawList* pDrawList, const glm::vec4& viewport)
{
    g_pDrawList = pDrawList;
    g_viewPort = viewport;

    /*
    vm->exec("a = [1, 2, 3]");
    auto result = vm->eval("sum(a)");
    LOG(DBG, "Result: " << CAST(int, result));

    vm->exec("circle(100.0, 100.0, 50.0, 10.0)");
    */

    g_pDrawList = nullptr;
}

void python_destroy()
{
//    delete vm;
}

std::shared_ptr<PythonModule> python_compile(const fs::path& path)
{
    auto spModule = std::make_shared<PythonModule>();
    spModule->path = path;
    spModule->pVM = make_vm();
    spModule->script = Zest::file_read(path);
    return nullptr;
}

void python_run(PythonModule& mod)
{
    auto result = mod.pVM->eval(mod.script);
}
