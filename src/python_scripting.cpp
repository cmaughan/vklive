#include <pocketpy/pocketpy.h>

#include <regex>
// #include <set>
// #include <cstring>
// #include <fmt/format.h>
// #include <fstream>
// #include <sstream>

#include <vklive/python_scripting.h>
#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <zest/logger/logger.h>
#include <zest/string/string_utils.h>
#include <zest/ui/fonts.h>

#include <glm/glm.hpp>

#include <vklive/IDevice.h>
#include <vklive/scene.h>

#include <zest/ui/nanovg.h>

#include <imgui.h>

// using namespace pkpy;
using namespace Zest;
using namespace vulkan;

namespace
{
// TODO: Thread safe?
// pkpy::VM* vm = nullptr;
ImDrawList* g_pDrawList = nullptr;
Scene* g_pScene = nullptr;
IDevice* g_pDevice = nullptr;
NVGcontext* g_vg = nullptr;

glm::vec4 g_viewPort = glm::vec4(0.0f);
std::mutex pyMutex;
} // namespace

bool python_parse_output(const std::string& strOutput, const fs::path& shaderPath, Scene& scene)
{
    bool errors = false;
    if (strOutput.empty())
    {
        return errors;
    }

    auto p = fs::canonical(shaderPath);

    std::map<int32_t, std::vector<Message>> messageLines;

    std::vector<Message> noLineMessages;
    auto error_lines = Zest::string_split(strOutput, "\r\n");

    Message msg;
    msg.severity = MessageSeverity::Error;
    msg.path = p;
    errors = true;

    for (auto& error_line : error_lines)
    {
        try
        {
            std::regex pathRegex(".*\"(.*)\".*line.* ([0-9]+)", std::regex::icase);

            std::smatch match;
            if (std::regex_search(error_line, match, pathRegex) && match.size() > 1)
            {
                msg.path = Zest::string_trim(match[1].str());
                msg.line = std::stoi(match[2].str()) - 1;
            }
            else
            {
                msg.text += error_line;
                msg.text += "\n";
            }
        }
        catch (...)
        {
            msg.text = "Failed to parse python error:\n" + error_line;
            msg.line = -1;
            msg.severity = MessageSeverity::Error;
        }
    }

    scene_report_error(scene, msg.severity, msg.text, msg.path, msg.line);

    return errors;
}

std::shared_ptr<VM> make_vm()
{
    using namespace Zest;
    auto vm = std::make_shared<pkpy::VM>();

    vm->bind(vm->_main, "text(pos: vec2, text: string, col: vec4, size: int)", [](VM* vm, ArgsView args) {
        auto pos = CAST(PyVec2, args[0]);
        auto text = CAST(Str&, args[1]);
        auto col = CAST(PyVec4, args[2]);
        auto sz = CAST(int, args[3]);

        nvgFontSize(g_vg, sz);
        nvgFillColor(g_vg, NVGcolor{ col.x, col.y, col.z, col.w });
        nvgText(g_vg, pos.x, pos.y, text.c_str(), nullptr);

        return vm->None;
    });

    vm->bind(vm->_main, "hsv_to_rgb(vec4) -> vec4", [](VM* vm, ArgsView args) {
        auto out = CAST(PyVec4, args[0]);
        ImGui::ColorConvertHSVtoRGB(out.x, out.y, out.z, out.x, out.y, out.z);
        return VAR(out);
    });

    vm->bind(vm->_main, "time() -> float", [](VM* vm, ArgsView args) {
        return VAR(float(g_pScene->GlobalElapsedSeconds));
    });
    vm->bind(vm->_main, "circle(pos: vec2, col: vec4, radius: float, thickness: float)", [](VM* vm, ArgsView args) {
        auto pos = CAST(PyVec2, args[0]);
        auto col = CAST(PyVec4, args[1]);
        float rad = CAST(float, args[2]);
        float thickness = CAST(float, args[3]);

        pos.x += 1.0f;
        pos.y += 1.0f;
        pos.x *= (g_viewPort.z * .5);
        pos.y *= (g_viewPort.w * .5);

        pos.x += g_viewPort.x;
        pos.y += g_viewPort.y;

        nvgBeginPath(g_vg);
        nvgCircle(g_vg, pos.x, pos.y, rad);
        nvgFillColor(g_vg, NVGcolor{ col.x, col.y, col.z, col.w });
        nvgFill(g_vg);
        return vm->None;
    });

    vm->bind(vm->_main, "line(start: vec2, end: vec2, color: vec4, thickness: float)", [](VM* vm, ArgsView args) {
        auto start = CAST(PyVec2, args[0]);
        auto end = CAST(PyVec2, args[1]);
        auto col = CAST(PyVec4, args[2]);
        float thickness = CAST(float, args[3]);

        start.x += 1.0f;
        start.y += 1.0f;
        start.x *= (g_viewPort.z * .5);
        start.y *= (g_viewPort.w * .5);
        start.x += g_viewPort.x;
        start.y += g_viewPort.y;
        
        end.x += 1.0f;
        end.y += 1.0f;
        end.x *= (g_viewPort.z * .5);
        end.y *= (g_viewPort.w * .5);
        end.x += g_viewPort.x;
        end.y += g_viewPort.y;

        nvgBeginPath(g_vg);
        nvgStrokeColor(g_vg, NVGcolor{ col.x, col.y, col.z, col.w });
        nvgStrokeWidth(g_vg, thickness);
        nvgMoveTo(g_vg, start.x, start.y);
        nvgLineTo(g_vg, end.x, end.y);
        nvgStroke(g_vg);
        return vm->None;
    });
    
    vm->bind(vm->_main, "bezier(p1: vec2, p2: vec2, p3: vec2, p4: vec2, color: vec4, thickness: float)", [](VM* vm, ArgsView args) {
        ImVec2 points[4];
        for (int i = 0; i < 4; i++)
        {
            PyVec2 pt = CAST(PyVec2, args[i]);
            points[i] = ImVec2(pt.x + g_viewPort.x, pt.y + g_viewPort.y);
        }
        auto col = CAST(PyVec4, args[4]);
        float thickness = CAST(float, args[5]);

        nvgBeginPath(g_vg);
        nvgStrokeColor(g_vg, NVGcolor{ col.x, col.y, col.z, col.w });
        nvgStrokeWidth(g_vg, thickness);
        nvgMoveTo(g_vg, points[0].x, points[1].y);
        nvgBezierTo(g_vg, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y);
        nvgStroke(g_vg);
        return vm->None;
    });

    vm->bind(vm->_main, "rect(start: vec2, end: vec2, color: vec4, round: float, thickness: float)", [](VM* vm, ArgsView args) {
        auto start = CAST(PyVec2, args[0]);
        auto end = CAST(PyVec2, args[1]);
        auto col = CAST(PyVec4, args[2]);
        float thickness = CAST(float, args[3]);

        /* start.x += 1.0f;
        start.y += 1.0f;
        start.x *= (g_viewPort.z * .5);
        start.y *= (g_viewPort.w * .5);
        start.x += g_viewPort.x;
        start.y += g_viewPort.y;
        
        end.x += 1.0f;
        end.y += 1.0f;
        end.x *= (g_viewPort.z * .5);
        end.y *= (g_viewPort.w * .5);
        end.x += g_viewPort.x;
        end.y += g_viewPort.y;*/

        nvgBeginPath(g_vg);
        nvgStrokeColor(g_vg, NVGcolor{ col.x, col.y, col.z, col.w });
        nvgStrokeWidth(g_vg, thickness);
        nvgRect(g_vg, start.x, start.y, end.x - start.x, end.y - start.y);
        nvgStroke(g_vg);
        return vm->None;
    });
    return vm;
}

void python_destroy()
{
    //    delete vm;
}

std::shared_ptr<PythonModule> python_compile(Scene& scene, const fs::path& path)
{
    std::lock_guard<std::mutex> lk(pyMutex);
    auto spModule = std::make_shared<PythonModule>();
    spModule->path = path;
    spModule->spVM = make_vm();
    spModule->script = Zest::file_read(path);

    try
    {
        spModule->spCode = spModule->spVM->compile(spModule->script, path.string(), EXEC_MODE);
        if (spModule->spCode)
        {
            spModule->spVM->_exec(spModule->spCode, spModule->spVM->_main);
        }
    }
    catch (Exception& ex)
    {
        LOG(DBG, ex.summary());
        python_parse_output(ex.summary().str(), path, scene);
        spModule->spVM.reset();
    }
    catch (std::exception& ex)
    {
        LOG(DBG, ex.what());
        python_parse_output(ex.what(), path, scene);
        spModule->spVM.reset();
    }

    return spModule;
}

bool python_run_2d(PythonModule& mod, IDevice* pDevice, Scene& scene, ImDrawList* pDrawList, const glm::vec4& viewport)
{
    if (!mod.spCode)
    {
        return true;
    }
    std::lock_guard<std::mutex> lk(pyMutex);
    g_pDrawList = pDrawList;
    g_viewPort = viewport;
    g_pScene = &scene;
    g_pDevice = pDevice;

    return true;
    /*
    try
    {
        //mod.spVM->_exec(mod.spCode, mod.spVM->_main);
        auto draw = mod.spVM->eval("draw_2d");
        if (draw)
        {
            mod.spVM->call(draw);
        }
    }
    catch (Exception& ex)
    {
        LOG(DBG, ex.summary());
        python_parse_output(ex.summary().str(), mod.path, scene);
        mod.spCode.reset();
        mod.spVM.reset();
        return false;
    }
    catch (std::exception& ex)
    {
        LOG(DBG, ex.what());
        python_parse_output(ex.what(), mod.path, scene);
        mod.spCode.reset();
        mod.spVM.reset();
        return false;
    }

    g_pDrawList = nullptr;
    g_pScene = nullptr;
    g_pDevice = nullptr;
    return true;
    */
}

void python_run_pass(NVGcontext* vg, Pass& pass, const glm::uvec2& targetSize)
{
    if (pass.script.empty())
    {
        return;
    }

    auto itr = pass.scene.scripts.find(pass.script);
    if (itr == pass.scene.scripts.end())
    {
        return;
    }

    auto& mod = *itr->second;
    if (!mod.spCode)
    {
        return;
    }

    std::lock_guard<std::mutex> lk(pyMutex);
    g_pDrawList = nullptr;
    g_pDevice = nullptr;
    g_viewPort = glm::vec4(0.0f, 0.0f, targetSize.x, targetSize.y);
    g_pScene = &pass.scene;
    g_vg = vg;

    try
    {
        auto draw = mod.spVM->eval(pass.entry.c_str());
        if (draw)
        {
            mod.spVM->call(draw);
        }
    }
    catch (Exception& ex)
    {
        LOG(DBG, ex.summary());
        python_parse_output(ex.summary().str(), mod.path, pass.scene);
        mod.spCode.reset();
        mod.spVM.reset();
    }
    catch (std::exception& ex)
    {
        LOG(DBG, ex.what());
        python_parse_output(ex.what(), mod.path, pass.scene);
        mod.spCode.reset();
        mod.spVM.reset();
    }

    g_pDrawList = nullptr;
    g_pScene = nullptr;
    g_pDevice = nullptr;
    g_vg = nullptr;

    /*
    NVGcolor col;
    col.r = 1.0f;
    col.a = 1.0f;
    nvgFillColor(ctx.vg, col);
    nvgCircle(ctx.vg, 100.0f, 100.0f, 50.0f);
    nvgFill(ctx.vg);

    col.g = 1.0f;
    nvgFillColor(ctx.vg, col);
    nvgFontSize(ctx.vg, 90.0);
    nvgText(ctx.vg, 100, 200, "Hello", nullptr);
    */
}
