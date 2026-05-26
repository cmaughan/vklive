#include <vklive_nvim/nvim_host.h>

namespace vklive_nvim
{
namespace
{

std::string escape_vim_path(const std::filesystem::path& path)
{
    const std::string value = path.generic_string();
    std::string escaped;
    escaped.reserve(value.size());

    for (char c : value)
    {
        switch (c)
        {
        case ' ':
        case '\\':
        case '|':
        case '"':
        case '%':
        case '#':
            escaped.push_back('\\');
            break;
        default:
            break;
        }
        escaped.push_back(c);
    }

    return escaped;
}

} // namespace

std::vector<std::string> build_open_project_tab_commands(const NvimProjectFiles& project)
{
    std::vector<std::string> commands;
    commands.reserve(project.files.size() + 2);
    commands.emplace_back("silent! tabonly");

    for (const auto& file : project.files)
    {
        commands.push_back("tabedit " + escape_vim_path(file));
    }

    if (!project.files.empty())
    {
        commands.emplace_back("tabfirst");
    }

    return commands;
}

} // namespace vklive_nvim
