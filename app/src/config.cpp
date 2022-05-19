#include <sstream>
#include "toml++/toml.h"

#include "app/config.h"

#include <vklive/logger/logger.h>
#include <vklive/file/file.h>

AppConfig appConfig;

template<class T>
glm::vec2 read_vec2(const T& node, const glm::vec2& def = glm::vec2(0.0f))
{
    using namespace std::string_view_literals;
    toml::array* pArray = node.as_array();
    if (!pArray)
    {
        return def; 
    }

    glm::vec2 ret;
    ret.x = (*pArray)[0].value_or(def.x);
    ret.y = (*pArray)[1].value_or(def.y);
    return ret;
}

void write_vec2(toml::table& table, const std::string& strEntry, const glm::vec2& value)
{
    table.insert_or_assign(strEntry, toml::array{ value.x, value.y });
}

void config_load(const fs::path& path)
{
    toml::table tbl;
    try
    {
        tbl = toml::parse_file(path.string());
        LOG(INFO, "Config:\n" << tbl);

        auto projectPath = fs::path(tbl["settings"]["project_root"].value_or(""));
        if (fs::is_directory(projectPath))
        {
            appConfig.project_root = fs::canonical(projectPath);
        }
        appConfig.vim_mode = tbl["settings"]["vim_mode"].value_or(false);

        appConfig.main_window_size = read_vec2(tbl["settings"]["main_window_size"]);
        appConfig.main_window_pos = read_vec2(tbl["settings"]["main_window_pos"]);
        appConfig.main_window_state = WindowState(tbl["settings"]["main_window_state"].value_or(int(WindowState::Normal)));
        
        appConfig.draw_on_background = tbl["settings"]["draw_on_background"].value_or(false);
        appConfig.transparent_editor = tbl["settings"]["transparent_editor"].value_or(false);
    }
    catch (const toml::parse_error& err)
    {
    
    }
}

void config_save(const fs::path& path)
{
    toml::table settings;
    settings.insert_or_assign("project_root", appConfig.project_root.string());

    // Window
    write_vec2(settings, "main_window_size", appConfig.main_window_size);
    write_vec2(settings, "main_window_pos", appConfig.main_window_pos);
    settings.insert_or_assign("main_window_state", int(appConfig.main_window_state));

    settings.insert_or_assign("draw_on_background", appConfig.draw_on_background);
    settings.insert_or_assign("transparent_editor", appConfig.transparent_editor);

    toml::table tbl;
    tbl.insert("settings", settings);

    std::ostringstream str;
    str << tbl;

    file_write(path, str.str());

}