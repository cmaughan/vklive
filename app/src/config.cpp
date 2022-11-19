#include <sstream>
#include <vklive/file/toml_utils.h>

#include "app/config.h"

#include <vklive/logger/logger.h>
#include <vklive/file/file.h>
#include <vklive/file/toml_utils.h>
#include <vklive/audio/audio.h>

AppConfig appConfig;

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

        appConfig.main_window_size = toml_read_vec2<glm::vec2>(tbl["settings"]["main_window_size"]);
        appConfig.main_window_pos = toml_read_vec2<glm::vec2>(tbl["settings"]["main_window_pos"]);
        appConfig.main_window_state = WindowState(tbl["settings"]["main_window_state"].value_or(int(WindowState::Normal)));
        appConfig.last_folder_path = fs::path(tbl["settings"]["last_folder_path"].value_or(""));
        
        appConfig.draw_on_background = tbl["settings"]["draw_on_background"].value_or(false);
        appConfig.transparent_editor = tbl["settings"]["transparent_editor"].value_or(false);

        auto pAnalysisTable = tbl["settings"]["audio_analysis"].as_table();
        auto pDeviceTable = tbl["settings"]["audio_device"].as_table();
        if (pAnalysisTable)
        {
            appConfig.audioAnalysisSettings = Audio::audioanalysis_load_settings(*pAnalysisTable);
        }

        if (pDeviceTable)
        {
            appConfig.audioDeviceSettings = Audio::audiodevice_load_settings(*pDeviceTable);
        }
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
    toml_write_vec2(settings, "main_window_size", appConfig.main_window_size);
    toml_write_vec2(settings, "main_window_pos", appConfig.main_window_pos);
    settings.insert_or_assign("main_window_state", int(appConfig.main_window_state));

    settings.insert_or_assign("draw_on_background", appConfig.draw_on_background);
    settings.insert_or_assign("transparent_editor", appConfig.transparent_editor);

    settings.insert_or_assign("last_folder_path", appConfig.last_folder_path.string());

    toml::table analysis_settings = Audio::audioanalysis_save_settings(Audio::GetAudioContext().audioAnalysisSettings);
    toml::table device_settings = Audio::audiodevice_save_settings(Audio::GetAudioContext().audioDeviceSettings);

    settings.insert_or_assign("audio_analysis", analysis_settings);
    settings.insert_or_assign("audio_device", device_settings);
    
    toml::table tbl;
    tbl.insert("settings", settings);

    std::ostringstream str;
    str << tbl;

    file_write(path, str.str());

}