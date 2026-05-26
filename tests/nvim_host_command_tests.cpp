#include <vklive_nvim/nvim_host.h>

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

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

    assert(commands == std::vector<std::string>({
                           "silent! tabonly",
                           "tabedit D:/projects/demo/shaders/a.frag",
                           "tabedit D:/projects/demo/shaders/space\\ shader.vert",
                           "tabedit D:/projects/demo/shaders/pipe\\|shader.frag",
                           "tabfirst",
                       }));
}
