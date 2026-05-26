#include <app/config.h>
#include <app/editor_backend.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <zest/logger/logger.h>

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
}

int main()
{
    const auto path = std::filesystem::temp_directory_path() / "vklive_editor_backend_config.toml";
    {
        std::ofstream out(path);
        out << "[settings]\n";
        out << "editor_backend = \"neovim\"\n";
    }

    appConfig = AppConfig{};
    config_load(path);
    assert(appConfig.editor_backend == EditorBackendKind::Neovim);

    appConfig.editor_backend = EditorBackendKind::Zep;
    config_save(path);

    {
        std::ifstream in(path);
        const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(saved.find("editor_backend = 'zep'") != std::string::npos || saved.find("editor_backend = \"zep\"") != std::string::npos);
    }

    std::filesystem::remove(path);
}
