#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>

#include <app/command_line.h>

namespace
{
bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}
}

int main()
{
    AppCommandLineOptions options;
    std::string error;

    const char* argv[] = {
        "Rezonality.exe",
        "--project",
        "run_tree/projects/pbr_robot",
        "--scenegraph",
        "uv_debug.scenegraph"
    };

    bool ok = app_parse_command_line(static_cast<int>(std::size(argv)), const_cast<char**>(argv), options, error);

    ok &= require(error.empty(), "parser returned error: " + error);
    ok &= require(options.projectRoot == fs::path("run_tree/projects/pbr_robot"), "project path not parsed");
    ok &= require(options.sceneGraph == fs::path("uv_debug.scenegraph"), "scenegraph path not parsed");

    AppCommandLineOptions badOptions;
    const char* badArgv[] = { "Rezonality.exe", "--scenegraph" };
    std::string badError;
    const bool badOk = app_parse_command_line(static_cast<int>(std::size(badArgv)), const_cast<char**>(badArgv), badOptions, badError);
    ok &= require(!badOk, "missing scenegraph value should fail");
    ok &= require(!badError.empty(), "missing scenegraph value should explain failure");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
