#pragma once

#include <string>

#include <zest/file/file.h>

struct AppCommandLineOptions
{
    fs::path projectRoot;
    fs::path sceneGraph;
    bool smokeTest = false;
    bool help = false;
};

bool app_parse_command_line(int argc, char** argv, AppCommandLineOptions& options, std::string& error);
std::string app_command_line_help();
