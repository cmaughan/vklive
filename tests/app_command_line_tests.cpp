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
    ok &= require(!options.rendererSpecified, "renderer should not be specified by default");

    AppCommandLineOptions badOptions;
    const char* badArgv[] = { "Rezonality.exe", "--scenegraph" };
    std::string badError;
    const bool badOk = app_parse_command_line(static_cast<int>(std::size(badArgv)), const_cast<char**>(badArgv), badOptions, badError);
    ok &= require(!badOk, "missing scenegraph value should fail");
    ok &= require(!badError.empty(), "missing scenegraph value should explain failure");

    const char* rendererArgv[] = {
        "Rezonality.exe",
        "--renderer",
        "metal",
        "--smoke-test"
    };
    AppCommandLineOptions rendererOptions;
    std::string rendererError;
    bool rendererOk = app_parse_command_line(static_cast<int>(std::size(rendererArgv)), const_cast<char**>(rendererArgv), rendererOptions, rendererError);
    ok &= require(rendererOk, "renderer parser failed: " + rendererError);
    ok &= require(rendererOptions.rendererSpecified, "metal renderer should be marked specified");
    ok &= require(rendererOptions.renderer == RenderBackend::Metal, "metal renderer not parsed");
    ok &= require(rendererOptions.smokeTest, "smoke-test not parsed after renderer");

    const char* autoRendererArgv[] = {
        "Rezonality.exe",
        "--renderer",
        "auto"
    };
    AppCommandLineOptions autoRendererOptions;
    std::string autoRendererError;
    bool autoRendererOk = app_parse_command_line(static_cast<int>(std::size(autoRendererArgv)), const_cast<char**>(autoRendererArgv), autoRendererOptions, autoRendererError);
    ok &= require(autoRendererOk, "auto renderer parser failed: " + autoRendererError);
    ok &= require(autoRendererOptions.rendererSpecified, "auto renderer should be marked specified");
    ok &= require(autoRendererOptions.renderer == RenderBackend::Auto, "auto renderer not parsed");

    const char* badRendererArgv[] = {
        "Rezonality.exe",
        "--renderer",
        "direct3d"
    };
    AppCommandLineOptions badRendererOptions;
    std::string badRendererError;
    bool badRendererOk = app_parse_command_line(static_cast<int>(std::size(badRendererArgv)), const_cast<char**>(badRendererArgv), badRendererOptions, badRendererError);
    ok &= require(!badRendererOk, "bad renderer should fail");
    ok &= require(badRendererError == "Unknown renderer: direct3d", "bad renderer error text changed");

    const char* startupFrameArgv[] = {
        "Rezonality.exe",
        "--renderer",
        "metal",
        "--startup-frame-test"
    };
    AppCommandLineOptions startupFrameOptions;
    std::string startupFrameError;
    bool startupFrameOk = app_parse_command_line(static_cast<int>(std::size(startupFrameArgv)), const_cast<char**>(startupFrameArgv), startupFrameOptions, startupFrameError);
    ok &= require(startupFrameOk, "startup-frame parser failed: " + startupFrameError);
    ok &= require(startupFrameOptions.renderer == RenderBackend::Metal, "startup-frame renderer not parsed");
    ok &= require(startupFrameOptions.startupFrameTest, "startup-frame flag not parsed");

    const char* macPersistenceArgv[] = {
        "Rezonality.exe",
        "-ApplePersistenceIgnoreState",
        "YES",
        "--startup-frame-test"
    };
    AppCommandLineOptions macPersistenceOptions;
    std::string macPersistenceError;
    bool macPersistenceOk = app_parse_command_line(static_cast<int>(std::size(macPersistenceArgv)), const_cast<char**>(macPersistenceArgv), macPersistenceOptions, macPersistenceError);
    ok &= require(macPersistenceOk, "ApplePersistenceIgnoreState parser failed: " + macPersistenceError);
    ok &= require(macPersistenceOptions.startupFrameTest, "startup-frame flag not parsed after ApplePersistenceIgnoreState");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
