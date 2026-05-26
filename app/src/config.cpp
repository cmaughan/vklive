#include <sstream>

#include <zest/file/toml_utils.h>
#include <zest/logger/logger.h>
#include <zest/file/file.h>
#include <zest/file/toml_utils.h>

#include <zing/audio/audio.h>

#include "app/config.h"

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

        EditorBackendKind editorBackend = appConfig.editor_backend;
        const auto editorBackendText = tbl["settings"]["editor_backend"].value_or(std::string(editor_backend_to_string(appConfig.editor_backend)));
        if (editor_backend_from_string(editorBackendText, editorBackend))
        {
            appConfig.editor_backend = editorBackend;
        }

        RenderBackend renderer = RenderBackend::Auto;
        const auto rendererText = tbl["settings"]["renderer"].value_or("auto");
        if (render_backend_from_string(rendererText, renderer))
        {
            appConfig.renderer = renderer;
        }

        appConfig.main_window_size = toml_read_vec2<glm::vec2>(tbl["settings"]["main_window_size"]);
        appConfig.main_window_pos = toml_read_vec2<glm::vec2>(tbl["settings"]["main_window_pos"]);
        appConfig.main_window_state = WindowState(tbl["settings"]["main_window_state"].value_or(int(WindowState::Normal)));
        appConfig.last_folder_path = fs::path(tbl["settings"]["last_folder_path"].value_or(""));
        
        appConfig.draw_on_background = tbl["settings"]["draw_on_background"].value_or(false);
        appConfig.transparent_editor = tbl["settings"]["transparent_editor"].value_or(false);
        appConfig.viewports = tbl["settings"]["viewports"].value_or(false);

        auto pAnalysisTable = tbl["settings"]["audio_analysis"].as_table();
        auto pDeviceTable = tbl["settings"]["audio_device"].as_table();
        if (pAnalysisTable)
        {
            appConfig.audioAnalysisSettings = Zing::audioanalysis_load_settings(*pAnalysisTable);
        }

        if (pDeviceTable)
        {
            appConfig.audioDeviceSettings = Zing::audiodevice_load_settings(*pDeviceTable);
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
    settings.insert_or_assign("editor_backend", editor_backend_to_string(appConfig.editor_backend));
    settings.insert_or_assign("renderer", render_backend_to_string(appConfig.renderer));

    // Window
    toml_write_vec2(settings, "main_window_size", appConfig.main_window_size);
    toml_write_vec2(settings, "main_window_pos", appConfig.main_window_pos);
    settings.insert_or_assign("main_window_state", int(appConfig.main_window_state));

    settings.insert_or_assign("draw_on_background", appConfig.draw_on_background);
    settings.insert_or_assign("transparent_editor", appConfig.transparent_editor);
    settings.insert_or_assign("viewports", appConfig.viewports);

    settings.insert_or_assign("last_folder_path", appConfig.last_folder_path.string());

    toml::table analysis_settings = Zing::audioanalysis_save_settings(Zing::GetAudioContext().audioAnalysisSettings);
    toml::table device_settings = Zing::audiodevice_save_settings(Zing::GetAudioContext().audioDeviceSettings);

    settings.insert_or_assign("audio_analysis", analysis_settings);
    settings.insert_or_assign("audio_device", device_settings);
    
    toml::table tbl;
    tbl.insert("settings", settings);

    std::ostringstream str;
    str << tbl;

    Zest::file_write(path, str.str());

}
