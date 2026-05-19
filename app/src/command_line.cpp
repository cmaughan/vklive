#include "app/command_line.h"

#include <fmt/format.h>

std::string app_command_line_help()
{
    return R"(Usage:
  Rezonality [--project <dir>] [--scenegraph <file>] [--smoke-test]

Options:
  --project <dir>      Load a project directory for this launch.
  --scenegraph <file>  Override the project's scenegraph for this launch.
  --smoke-test         Start and exit immediately after command-line parsing.
)";
}

bool app_parse_command_line(int argc, char** argv, AppCommandLineOptions& options, std::string& error)
{
    options = {};
    error.clear();

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        auto requireValue = [&](const std::string& option) -> const char* {
            if (i + 1 >= argc || argv[i + 1] == nullptr || std::string(argv[i + 1]).rfind("--", 0) == 0)
            {
                error = fmt::format("{} requires a value", option);
                return nullptr;
            }
            ++i;
            return argv[i];
        };

        if (arg == "--project")
        {
            const char* value = requireValue(arg);
            if (!value)
            {
                return false;
            }
            options.projectRoot = value;
        }
        else if (arg == "--scenegraph")
        {
            const char* value = requireValue(arg);
            if (!value)
            {
                return false;
            }
            options.sceneGraph = value;
        }
        else if (arg == "--smoke-test")
        {
            options.smokeTest = true;
        }
        else if (arg == "-h" || arg == "--help")
        {
            options.help = true;
        }
        else
        {
            error = fmt::format("Unknown argument: {}", arg);
            return false;
        }
    }

    return true;
}
