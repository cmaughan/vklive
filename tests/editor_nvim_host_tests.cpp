#include <app/editor_nvim_host.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include <zest/logger/logger.h>

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
} // namespace Zest

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "vklive_nvim_editor_files";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "shaders");

    {
        std::ofstream(root / "main.scenegraph") << "scene\n";
        std::ofstream(root / "notes.txt") << "ignore\n";
        std::ofstream(root / "shaders" / "a.frag") << "frag\n";
        std::ofstream(root / "shaders" / "b.vert") << "vert\n";
    }

    const auto files = nvim_editor_collect_edit_files(root);
    assert(files.size() == 3);

    std::string joined;
    for (const auto& file : files)
    {
        joined += file.generic_string();
        joined += "\n";
    }

    assert(joined.find("main.scenegraph") != std::string::npos);
    assert(joined.find("a.frag") != std::string::npos);
    assert(joined.find("b.vert") != std::string::npos);
    assert(joined.find("notes.txt") == std::string::npos);

    std::filesystem::remove_all(root);
}
