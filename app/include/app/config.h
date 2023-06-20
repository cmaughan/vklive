#pragma once

#include <glm/glm.hpp>

#include <zest/file/file.h>

#include <zing/audio/audio_analysis_settings.h>
#include <zing/audio/audio_device_settings.h>

enum class WindowState
{
    Normal = 0,
    Maximized = 1,
    Minimized = 2
};

struct AppConfig
{
    bool vim_mode = false;
    fs::path project_root; 
    bool viewports = false;

    // Rendering type
    bool draw_on_background = false;
    bool transparent_editor = false;

    glm::vec2 main_window_pos = glm::vec2(0.0f);
    glm::vec2 main_window_size = glm::vec2(0.0f);
   
    // 0 = normal, 1 = maximized, 2 = minimized
    WindowState main_window_state = WindowState::Normal;

    fs::path last_folder_path;

    Zing::AudioAnalysisSettings audioAnalysisSettings;
    Zing::AudioDeviceSettings audioDeviceSettings;

};

extern AppConfig appConfig;

void config_load(const fs::path& path);
void config_save(const fs::path& path);