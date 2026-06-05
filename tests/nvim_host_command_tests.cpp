#include <vklive_nvim/nvim_host.h>

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

bool require_commands(const std::vector<std::string>& actual, const std::vector<std::string>& expected)
{
    if (actual == expected)
    {
        return true;
    }

    std::cerr << "Expected commands:\n";
    for (const auto& command : expected)
    {
        std::cerr << "  " << command << "\n";
    }

    std::cerr << "Actual commands:\n";
    for (const auto& command : actual)
    {
        std::cerr << "  " << command << "\n";
    }

    return false;
}

} // namespace

int main()
{
    vklive_nvim::NvimProjectFiles project;
    project.project_root = "D:/projects/demo";
    project.files = {
        "D:/projects/demo/shaders/a.frag",
        "D:/projects/demo/shaders/space shader.vert",
        "D:/projects/demo/shaders/pipe|shader.frag",
    };

    const auto commands = vklive_nvim::build_open_project_tab_commands(project);

    const bool ok = require_commands(commands, {
                                                   "silent! tabonly",
                                                   "edit D:/projects/demo/shaders/a.frag",
                                                   "tabedit D:/projects/demo/shaders/space\\ shader.vert",
                                                   "tabedit D:/projects/demo/shaders/pipe\\|shader.frag",
                                                   "tabfirst",
                                               });
    return ok ? 0 : 1;
}
