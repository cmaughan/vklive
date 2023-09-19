#include <pocketpy/pocketpy.h>
#include <zest/logger/logger.h>

#include <glm/glm.hpp>

#include <imgui.h>

using namespace pkpy;
using namespace Zest;

namespace
{
VM* vm = nullptr;
ImDrawList* g_pDrawList = nullptr;
glm::vec4 g_viewPort = glm::vec4(0.0f);
}

void python_init()
{
    vm = new VM();

    vm->bind(vm->_main, "circle(x: float, y: float, radius: float, thickness: float)", [](VM* vm, ArgsView args) {
        ImVec2 pos(CAST(float, args[0]), CAST(float, args[1]));
        float rad = CAST(float, args[2]);
        float thickness = CAST(float, args[3]);

        pos.x += g_viewPort.x;
        pos.y += g_viewPort.y;

        g_pDrawList->AddCircle(pos, rad, 0xFFFFFFFF, 0, thickness);
        return vm->None;
    });


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
    delete vm;
}