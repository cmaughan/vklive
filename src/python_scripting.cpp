#include <pocketpy/pocketpy.h>

#include <regex>
//#include <set>
//#include <cstring>
//#include <fmt/format.h>
//#include <fstream>
//#include <sstream>

#include <vklive/python_scripting.h>
#include <zest/logger/logger.h>
#include <zest/file/file.h>
#include <zest/string/string_utils.h>

#include <glm/glm.hpp>

#include <vklive/scene.h>

#include <imgui.h>

//using namespace pkpy;
using namespace Zest;

namespace
{
    // TODO: Thread safe?
//pkpy::VM* vm = nullptr;
ImDrawList* g_pDrawList = nullptr;
glm::vec4 g_viewPort = glm::vec4(0.0f);
Scene* g_pScene = nullptr;
std::mutex pyMutex;
}

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
    auto vm = std::make_shared<pkpy::VM>();

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

        pos.x += g_viewPort.x;
        pos.y += g_viewPort.y;

        auto imCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w));
        g_pDrawList->AddCircle(ImVec2(pos.x, pos.y), rad, imCol, 0, thickness);
        return vm->None;
    });
    
    vm->bind(vm->_main, "line(start: vec2, end: vec2, color: vec4, thickness: float)", [](VM* vm, ArgsView args) {
        auto start = CAST(PyVec2, args[0]);
        auto end = CAST(PyVec2, args[1]);
        auto col = CAST(PyVec4, args[2]);
        float thickness = CAST(float, args[3]);

        start.x += g_viewPort.x;
        start.y += g_viewPort.y;

        end.x += g_viewPort.x;
        end.y += g_viewPort.y;

        auto imCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w));
        g_pDrawList->AddLine(ImVec2(start.x, start.y), ImVec2(end.x, end.y), imCol, thickness);
        return vm->None;
    });
    
    vm->bind(vm->_main, "rect(start: vec2, end: vec2, color: vec4, round: float, thickness: float)", [](VM* vm, ArgsView args) {
        auto start = CAST(PyVec2, args[0]);
        auto end = CAST(PyVec2, args[1]);
        auto col = CAST(PyVec4, args[2]);
        float round = CAST(float, args[3]);
        float thickness = CAST(float, args[4]);

        start.x += g_viewPort.x;
        start.y += g_viewPort.y;

        end.x += g_viewPort.x;
        end.y += g_viewPort.y;

        auto imCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w));
        g_pDrawList->AddRect(ImVec2(start.x, start.y), ImVec2(end.x, end.y), imCol, round, 0, thickness);
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
    }
    catch (Exception& ex)
    {
        LOG(DBG, ex.summary());
        python_parse_output(ex.summary().str(), path, scene);
        spModule->spVM.reset();
    }
    catch(std::exception & ex)
    {
        LOG(DBG, ex.what());
        python_parse_output(ex.what(), path, scene);
        spModule->spVM.reset();
    }

    return spModule;
}

bool python_run_2d(PythonModule& mod, Scene& scene, ImDrawList* pDrawList, const glm::vec4& viewport)
{
    if (!mod.spCode)
    {
        return true;
    }
    std::lock_guard<std::mutex> lk(pyMutex);
    g_pDrawList = pDrawList;
    g_viewPort = viewport;
    g_pScene = &scene;

    try
    {
        mod.spVM->_exec(mod.spCode, mod.spVM->_main);
    }
    catch (Exception& ex)
    {
        LOG(DBG, ex.summary());
        python_parse_output(ex.summary().str(), mod.path, scene);
        mod.spCode.reset();
        mod.spVM.reset();
        return false;
    }
    catch(std::exception & ex)
    {
        LOG(DBG, ex.what());
        python_parse_output(ex.what(), mod.path, scene);
        mod.spCode.reset();
        mod.spVM.reset();
        return false;
    }

    g_pDrawList = nullptr;
    g_pScene = nullptr;
    return true;
}
