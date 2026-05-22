#pragma once

#include <string>

#include <vklive/render_backend.h>

#include <zest/file/file.h>

struct AppCommandLineOptions
{
    fs::path projectRoot;
    fs::path sceneGraph;
    RenderBackend renderer = RenderBackend::Auto;
    bool rendererSpecified = false;
    bool smokeTest = false;
    bool startupFrameTest = false;
    bool recordOneFrame = false;
    bool viewports = false;
    bool help = false;
};

bool app_parse_command_line(int argc, char** argv, AppCommandLineOptions& options, std::string& error);
std::string app_command_line_help();
