#include <imgui/imgui.h>

#include <SDL.h>
#include <fmt/format.h>

#include <app/menu.h>

#include <vklive/IDevice.h>
#include <vklive/audio/audio.h>
#include <vklive/file/file.h>
#include <vklive/file/runtree.h>
#include <vklive/scene.h>
#include <vklive/string/string_utils.h>
#include <vklive/time/timer.h>

#include <config_app.h>

#include <app/config.h>
#include <app/controller.h>
#include <app/editor.h>

extern IDevice* GetDevice();

namespace
{
enum class PopupType
{
    None,
    Audio
};
PopupType popupType = PopupType::None;
} // namespace

void show_audio_popup()
{
    auto dpi = 1.0f;
    if (GetDevice())
    {
        dpi = GetDevice()->Context().vdpi;
    }

    // TODO: Center
    ImGui::SetNextWindowSize(ImVec2(dpi * 500, dpi * 500), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Audio", ImGuiWindowFlags_Popup))
    {
        bool show = true;
        Audio::audio_show_gui();

        if (ImGui::Button("OK"))
        {
            show = false;
        }

        if (!show)
        {
            popupType = PopupType::None;
            ImGui::CloseCurrentPopup();
        }

        // ImGui::End();
        ImGui::EndPopup();
    }
}

void menu_show()
{
    enum class PopupSelection
    {
        None,
        Error,
        Audio,
    };
    PopupSelection selectedPopup = PopupSelection::None;

    // Simple menu options for switching mode and splitting
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::BeginMenu("New Project"))
            {
                // Opus::MakeDefaultOpus();
                if (ImGui::BeginMenu("From Template"))
                {
                    auto generateName = [=](auto& folder) {
                        std::string name = folder.stem().string();
                        name = string_tolower(name);
                        auto words = string_split(name, " _");
                        std::ostringstream str;
                        for (auto& word : words)
                        {
                            if (word.size() > 0)
                            {
                                word[0] = char(::toupper((int)word[0]));
                            }
                            str << word << " ";
                        }

                        return string_trim(str.str());
                    };

                    using fnAddFolder = std::function<void(const fs::path&)>;
                    fnAddFolder addFolder = [&](auto folder) -> void {
                        auto files = file_gather_files(folder, false);
                        auto name = generateName(folder);
                        if (!name.empty())
                        {
                            // No files, just more folders
                            if (files.empty())
                            {
                                if (ImGui::BeginMenu(name.c_str()))
                                {
                                    auto folders = file_gather_folders(folder);
                                    for (auto& f : folders)
                                    {
                                        addFolder(f);
                                    }
                                    ImGui::EndMenu();
                                }
                            }
                            else
                            {
                                if (ImGui::MenuItem(name.c_str()))
                                {
                                    auto spProject = project_load_to_temp(folder);
                                    g_Controller.spProjectQueue->enqueue(spProject);
                                }
                            }
                        }
                    };

                    auto folders = file_gather_folders(runtree_path() / "projects");
                    for (auto& f : folders)
                    {
                        addFolder(f);
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Open Project..."))
            {
                auto newPath = controller_open_project();
                if (!newPath.empty())
                {
                    auto pProject = project_load(newPath);
                    if (pProject)
                    {
                        g_Controller.spProjectQueue->enqueue(pProject);
                    }
                }
            }

            if (ImGui::MenuItem("Save Project As...", "", nullptr, g_Controller.spCurrentProject != nullptr))
            {
                auto newPath = controller_save_project_as();
                if (!newPath.empty())
                {
                    auto pProject = project_load(newPath);
                    if (pProject)
                    {
                        pProject->modified = false;
                        g_Controller.spProjectQueue->enqueue(pProject);
                    }
                }
            }

            if (ImGui::MenuItem("Close"))
            {
                if (zep_get_editor().GetActiveTabWindow())
                {
                    zep_get_editor().GetActiveTabWindow()->CloseActiveWindow();
                }
            }
            
            if (ImGui::MenuItem("Exit"))
            {
                SDL_Event ev;
                ev.type = SDL_QUIT;
                SDL_PushEvent(&ev);

            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Assets"))
        {
            if (g_Controller.spCurrentProject && g_Controller.spCurrentProject->spScene)
            {
                auto spScene = g_Controller.spCurrentProject->spScene;
                // Assets->shader.vs
                uint32_t items = 0;
                if (!spScene->shaders.empty())
                {
                    if (ImGui::BeginMenu("Shaders"))
                    {
                        std::vector<fs::path> files;
                        for (auto& [shaderPath, shader] : spScene->shaders)
                        {
                            items++;
                            if (ImGui::MenuItem(shaderPath.filename().string().c_str()))
                            {
                                zep_load(shaderPath, true);
                            }
                        }
                        ImGui::EndMenu();
                    }
                }

                // Assets->scene.sceneGraph
                if (!spScene->sceneGraphPath.empty())
                {
                    if (items != 0)
                    {
                        ImGui::Separator();
                    }
                    items++;
                    if (ImGui::MenuItem(spScene->sceneGraphPath.filename().string().c_str()))
                    {
                        zep_load(spScene->sceneGraphPath, true);
                    }
                }
            }
            ImGui::EndMenu();
        }

        // ImGui::SetNextWindowPosCenter(ImGuiCond_Appearing);
        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("Audio..."))
            {
                popupType = PopupType::Audio;
            }

            ImGui::MenuItem("Background Render", nullptr, &appConfig.draw_on_background, true);

            if (ImGui::BeginMenu("Background Options", appConfig.draw_on_background))
            {
                ImGui::MenuItem("Transparent editor", nullptr, &appConfig.transparent_editor, true);
                ImGui::EndMenu();
            }
            /*
            if (ImGui::MenuItem("Audio..."))
            {
                selectedPopup = PopupSelection::Audio;
            }
            if (ImGui::BeginMenu("Font Size"))
            {
                if (ImGui::MenuItem("Bigger"))
                {
                }
                else if (ImGui::MenuItem("Smaller"))
                {
                }
                ImGui::EndMenu();
            }
            */
            if (ImGui::BeginMenu("Editor Mode"))
            {
                bool enabledVim = strcmp(zep_get_editor().GetGlobalMode()->Name(), Zep::ZepMode_Vim::StaticName()) == 0;
                bool enabledNormal = !enabledVim;
                if (ImGui::MenuItem("Vim", "CTRL+2", &enabledVim))
                {
                    zep_get_editor().SetGlobalMode(Zep::ZepMode_Vim::StaticName());
                }
                else if (ImGui::MenuItem("Standard", "CTRL+1", &enabledNormal))
                {
                    zep_get_editor().SetGlobalMode(Zep::ZepMode_Standard::StaticName());
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Theme"))
            {
                bool enabledDark = zep_get_editor().GetTheme().GetThemeType() == Zep::ThemeType::Dark ? true : false;
                bool enabledLight = !enabledDark;

                if (ImGui::MenuItem("Dark", "", &enabledDark))
                {
                    zep_get_editor().GetTheme().SetThemeType(Zep::ThemeType::Dark);
                }
                else if (ImGui::MenuItem("Light", "", &enabledLight))
                {
                    zep_get_editor().GetTheme().SetThemeType(Zep::ThemeType::Light);
                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Restart Scene"))
            {
                globalFrameCount = 0;
                timer_restart(globalTimer);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Getting Started"))
            {
                zep_load(runtree_find_path("docs/intro.md"), true, Zep::FileFlags::ReadOnly | Zep::FileFlags::Locked);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            // DoLayoutMenu();
            auto pTabWindow = zep_get_editor().GetActiveTabWindow();
            if (ImGui::MenuItem("Horizontal Split"))
            {
                pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), Zep::RegionLayoutType::VBox);
            }
            else if (ImGui::MenuItem("Vertical Split"))
            {
                pTabWindow->AddWindow(&pTabWindow->GetActiveWindow()->GetBuffer(), pTabWindow->GetActiveWindow(), Zep::RegionLayoutType::HBox);
            }
            /*
            else if (ImGui::MenuItem("Reset Layout"))
            {
            }
            */
            ImGui::EndMenu();
        }

        // Display FPS on the menu
        float time = globalElapsedSeconds;

        static auto lastTime = time;
        static auto fps = 0.0;
        if ((time - lastTime) > 1.0)
        {
            fps = ImGui::GetIO().Framerate;
            lastTime = time;
        }

        auto strSceneData = fmt::format(" FPS: {:.0f} Frame: {}", fps, globalFrameCount);
        if (ImGui::BeginMenu(strSceneData.c_str()))
        {
            ImGui::EndMenu();
        }
        

        ImGui::EndMainMenuBar();

        /*
        if (editor.popupRequest == PopupRequest::Layout)
        {
            ImGui::OpenPopup("LayoutName");
            editor.popupRequest = PopupRequest::None;
        }
        DoLayoutNamePopup();
        */
    }

    switch (popupType)
    {
    default:
        break;
    case PopupType::Audio:
        ImGui::OpenPopup("Audio");
        break;
    };

    show_audio_popup();
}
