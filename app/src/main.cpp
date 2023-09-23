#include <atomic>
#include <mutex>
#include <thread>

// #include <clipp.h> Awaiting c++ 20 update, or switch to something else.
#include <tinyfiledialogs/tinyfiledialogs.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <zest/imgui/imgui.h>
#include <zest/imgui/imgui_impl_sdl2.h>

#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <zest/logger/logger.h>
#include <zest/time/timer.h>
#include <zest/ui/layout_manager.h>

#include <vklive/process/process.h>

#include <vklive/IDevice.h>
#include <vklive/scene.h>

#include <vklive/validation.h>

#include <vklive/python_scripting.h>

#include <zing/audio/audio.h>

#include <config_app.h>

#include <app/config.h>
#include <app/controller.h>
#include <app/editor.h>
#include <app/menu.h>
#include <app/project.h>
#include <app/window_render.h>
#include <app/window_targets.h>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view.hpp>

#ifdef _WIN32
// For console
#include <windows.h>
#endif

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
} // namespace Zest

Controller g_Controller;
// using namespace clipp;

// Global access to the device
std::shared_ptr<IDevice> g_pDevice = nullptr;
IDevice* GetDevice()
{
    return g_pDevice.get();
}

namespace vulkan
{
extern std::shared_ptr<IDevice> create_vulkan_device(SDL_Window* pWindow, const std::string& settingsPath, bool viewports = false);
}

namespace
{

bool read_command_line(int argc, char** argv, int& exitCode)
{
    /*
    auto cli = group(opt_value("viewports", appConfig.viewports));
    if (argc != 0)
    {
        parse(argc, argv, cli);
    }
    */
    return true;
}

SDL_Window* init_sdl_window()
{
    int xPos = (appConfig.main_window_pos.x == 0.0) ? SDL_WINDOWPOS_CENTERED : appConfig.main_window_pos.x;
    int yPos = (appConfig.main_window_pos.y == 0.0) ? SDL_WINDOWPOS_CENTERED : appConfig.main_window_pos.y;
    int xSize = (appConfig.main_window_size.x == 0.0) ? 1024 : appConfig.main_window_size.x;
    int ySize = (appConfig.main_window_size.y == 0.0) ? 768 : appConfig.main_window_size.y;

    // Setup window
    auto windowFlags = (SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (appConfig.main_window_state == WindowState::Maximized)
    {
        windowFlags |= SDL_WINDOW_MAXIMIZED;
    }
    else if (appConfig.main_window_state == WindowState::Minimized)
    {
        // TODO: For now, we don't allow starting minimized; it breaks the device setup
        // windowFlags |= SDL_WINDOW_MINIMIZED;
    }

    return SDL_CreateWindow("Rezonality", xPos, yPos, xSize, ySize, windowFlags);
}

void save_state()
{
    // If not temporary, remember what we last looked at
    if (g_Controller.spCurrentProject && !g_Controller.spCurrentProject->temporary)
    {
        // For reload next time
        appConfig.project_root = g_Controller.spCurrentProject->rootPath;
    }

    // Window Config
    if (g_pDevice)
    {
        int w, h, x, y;
        SDL_GetWindowSize(g_pDevice->Context().window, &w, &h);
        SDL_GetWindowPosition(g_pDevice->Context().window, &x, &y);
        appConfig.main_window_pos = glm::vec2(x, y);
        appConfig.main_window_size = glm::vec2(w, h);

        auto flags = SDL_GetWindowFlags(g_pDevice->Context().window);
        if (flags & SDL_WINDOW_MAXIMIZED)
        {
            appConfig.main_window_state = WindowState::Maximized;
        }
        else if (flags & SDL_WINDOW_MINIMIZED)
        {
            appConfig.main_window_state = WindowState::Minimized;
        }
        else
        {
            appConfig.main_window_state = WindowState::Normal;
        }
    }
}

void register_windows()
{
    Zest::layout_manager_register_window("Profiler", "Profiler", &g_WindowEnables.profiler);
    Zest::layout_manager_register_window("Targets", "Targets", &g_WindowEnables.targets);

    Zest::layout_manager_load_layouts_file("vklive", [](const std::string& name, const Zest::LayoutInfo& info) {
        if (!info.windowLayout.empty())
        {
            ImGui::LoadIniSettingsFromMemory(info.windowLayout.c_str());
        }
    });
}

void copy_scene_errors_to_zep(Scene& scene)
{
    // Report the errors, regardless
    for (auto& err : scene.errors)
    {
        if (!err.path.empty())
        {
            zep_add_file_message(err);
        }
    }
    scene.errors.clear();

    for (auto& err : scene.warnings)
    {
        if (!err.path.empty())
        {
            zep_add_file_message(err);
        }
    }
    scene.warnings.clear();
}

} // namespace

int main(int argc, char** argv)
{
    Zest::Profiler::Init();

#ifdef _WIN32
#ifdef _DEBUG
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        AllocConsole();
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), 0x07);
    }
#endif
#endif

    int exitCode = 0;
    if (!read_command_line(argc, argv, exitCode))
    {
        return exitCode;
    }

    // Asset paths
    Zest::runtree_init(SDL_GetBasePath(), VKLIVE_ROOT);

    // Get the settings
    auto settings_path = Zest::file_init_settings("VkLive",
        Zest::runtree_find_path("settings.toml"),
        fs::path("settings") / "settings.toml");
    config_load(settings_path);

    auto imSettingsPath = Zest::file_init_settings("VkLive",
        Zest::runtree_find_path("imgui.ini"),
        fs::path("settings") / "imgui.ini")
                              .string();

    // Set the audio config from the loaded config. We copy it back when closing the app
    Zing::GetAudioContext().audioAnalysisSettings = appConfig.audioAnalysisSettings;
    Zing::GetAudioContext().audioDeviceSettings = appConfig.audioDeviceSettings;

    // Project
    project_startup();

    // SDL (window management/input)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    Zest::timer_restart(Zest::globalTimer);

    // Main device
    g_pDevice = vulkan::create_vulkan_device(init_sdl_window(), imSettingsPath, appConfig.viewports);

    Zing::audio_init(nullptr);

    register_windows();

    // python_init();

    // This update thread generates a new scene, then returns it in a queue ready for 'swapping' with the existing one
    // if it is valid
    auto spQueue = std::make_shared<moodycamel::ConcurrentQueue<std::shared_ptr<Project>>>();
    g_Controller.spProjectQueue = std::make_shared<moodycamel::ConcurrentQueue<std::shared_ptr<Project>>>();

    std::atomic_bool quit_thread = false;
    std::thread update_thread = std::thread([&]() {
        const auto wakeUpDelta = std::chrono::milliseconds(10);
        for (;;)
        {
            // First check for quit
            if (quit_thread.load())
            {
                break;
            }

            std::shared_ptr<Project> spProject;
            if (!g_Controller.spProjectQueue->try_dequeue(spProject))
            {
                // Sleep
                std::this_thread::sleep_for(wakeUpDelta);
                continue;
            }

            spProject->spScene = scene_build(spProject->rootPath);

            // May not be valid, but sent anyway
            g_pDevice->InitScene(*spProject->spScene);

            // Queue the resulting initialized project & scene onto the UI thread
            spQueue->enqueue(spProject);
        }
    });

    // Startup, load the default project
    auto project = project_load(appConfig.project_root);
    g_Controller.spProjectQueue->enqueue(project);

    // Main loop
    bool done = false;
    uint32_t zepFocusFlags = 0;
    while (!done)
    {
        Zest::Profiler::NewFrame();

        PROFILE_NAME_THREAD(UI);

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                done = true;
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(g_pDevice->Context().window))
            {
                done = controller_check_exit();
            }

            if (event.type == SDL_KEYDOWN)
            {
                zepFocusFlags |= ZepFocusFlags::CheckFocus;
            }
        }

        // Attempt to recover a lost device: this does not work!
        // It is a hard problem to recover from some combinatoric errors of state.
        // We now rely on the validation layer to catch everything and ensure if it does, we don't draw.
        // So far, this protects from lost devices.
        // We may some day get this to work; but for now, once the device is gone, there seems to be no way to recover (
        // creating the device always fails)
        if (g_pDevice->Context().deviceState == DeviceState::Lost)
        {
            LOG(ERR, "Device Lost! (This shouldn't happen)");

            // Try to restart
            if (project_has_scene(g_Controller.spCurrentProject.get()))
            {
                g_pDevice->DestroyScene(*g_Controller.spCurrentProject->spScene.get());
            }
            g_pDevice = vulkan::create_vulkan_device(init_sdl_window(), imSettingsPath, appConfig.viewports);

            // Remember we were lost
            g_pDevice->Context().deviceState = DeviceState::WasLost;
        }
        g_pDevice->ValidateSwapChain();

        Zest::layout_manager_update();

        // Start the Dear ImGui frame
        ImGui_ImplSDL2_NewFrame(g_pDevice->Context().window);
        ImGui::NewFrame();

        if (menu_show())
        {
            zepFocusFlags &= ~(ZepFocusFlags::CheckFocus | ZepFocusFlags::Focus);
        }
        // ImGui::ShowDemoWindow();

        static bool update = false;
        static bool z_init = false;
        if (!z_init)
        {
            ZepEditorCB cb;
            cb.updateCB = [=](auto& buffer, auto& itr) {
                // Flash the buffer
                Zep::FlashType type = Zep::FlashType::Flash;

                buffer.BeginFlash(0.75f,
                    type,
                    Zep::GlyphRange(buffer.Begin(), buffer.End()));

                // Save the buffers
                if (buffer.HasFileFlags(Zep::FileFlags::Dirty))
                {
                    int64_t sz;
                    buffer.Save(sz);
                }

                if (project_scene_valid(g_Controller.spCurrentProject.get()))
                {
                    auto spScene = g_Controller.spCurrentProject->spScene;

                    auto updateFile = [](const auto& f) {
                        auto shader = zep_get_editor().FindFileBuffer(f.string());
                        if (shader && shader->HasFileFlags(Zep::FileFlags::Dirty))
                        {
                            int64_t sz;
                            shader->Save(sz);
                        }
                        return f;
                    };

                    ranges::for_each(spScene->shaders | ranges::views::keys, updateFile);
                    ranges::for_each(spScene->headers, updateFile);
                }

                // Make a new project from the existing and queue it for loading
                auto spProject = std::make_shared<Project>();
                spProject->rootPath = g_Controller.spCurrentProject->rootPath;
                spProject->temporary = g_Controller.spCurrentProject->temporary;
                spProject->modified = true;
                g_Controller.spProjectQueue->enqueue(spProject);
            };

            cb.formatCB = [=](auto& buffer, auto& itr) {
                zep_format_buffer(buffer, itr.Index());
            };

            // Called once the fonts/device is guaranteed setup
            zep_init(Zest::runtree_path(), Zep::NVec2f(1.0f, 1.0f), cb);

            z_init = true;
        }

        std::shared_ptr<Project> spNewProject;
        if (spQueue->try_dequeue(spNewProject))
        {
            LOG(DBG, "Deque Compiled Project");
            validation_enable_messages(true);

            zep_clear_all_messages();

            // Are we switching to a different project?
            bool switchProject = (!g_Controller.spCurrentProject || !fs::equivalent(g_Controller.spCurrentProject->rootPath, spNewProject->rootPath));

            // Do a 'pre-render'.  If this fails, then we will catch errors that might only happen during scene prepare
            if (project_scene_valid(spNewProject.get()))
            {
                LOG_SCOPE(DBG, "PreRender Compiled Project, scene: " << spNewProject->spScene.get());

                // Do a simple render, with no output, to a small area
                g_pDevice->Render_3D(*spNewProject->spScene, glm::vec2(10.0f, 10.0f));
            }

            // If the new project is still valid and has a working scene, swap
            // We still receive invalid projects, because we want to gather error info
            if (project_scene_valid(spNewProject.get()))
            {
                g_pDevice->WaitIdle();

                // Copy scene data and destroy
                if (project_scene_valid(g_Controller.spCurrentProject.get()))
                {
                    LOG_SCOPE(DBG, "Destroying previous scene: " << g_Controller.spCurrentProject->spScene.get());
                    g_pDevice->DestroyScene(*g_Controller.spCurrentProject->spScene);

                    // Copy over the old info, if appropriate - this is temporary fix for cleaner solution later.
                    scene_copy_state(*spNewProject->spScene, *g_Controller.spCurrentProject->spScene);
                }

                // Copy the new one
                g_Controller.spCurrentProject = spNewProject;

                // Back to a normal rendering state (see device lost comments)
                g_pDevice->Context().deviceState = DeviceState::Normal;
            }
            else
            {
                // We have switched projects, so even if the project doesn't load, we have to be in the mode to show
                // its files.  There will be no render, but the user can fix stuff
                if (switchProject)
                {
                    g_Controller.spCurrentProject = spNewProject;
                    zep_update_files(g_Controller.spCurrentProject->rootPath, switchProject);
                }

                // Otherwise, what we have is the same project, with the old render until it is fixed
                // (stopping the scene from disappearing when the user makes an edit mistake)
            }

            // Update the sources in the editor
            zep_update_files(g_Controller.spCurrentProject->rootPath, switchProject);

            // Reset the project messages (more global errors)
            g_Controller.spCurrentProject->projectMessages.clear();

            // Report the errors, regardless
            copy_scene_errors_to_zep(*spNewProject->spScene);

            zepFocusFlags |= ZepFocusFlags::Focus;
        }

        if (g_Controller.spCurrentProject)
        {
            // Some messages in the vulkan layer can be generated without any file context.
            // We might refine this later, but we collect the messages seperately and keep them for a 'project' message
            // dialog
            Message msg;
            while (validation_check_message_queue(msg))
            {
                if (!msg.path.empty())
                {
                    zep_add_file_message(msg);
                }
                else
                {
                    if (g_Controller.spCurrentProject->projectMessages.size() < 5)
                    {
                        g_Controller.spCurrentProject->projectMessages.push_back(msg);
                    }
                }
            }

            if (!g_Controller.spCurrentProject->projectMessages.empty())
            {
                if (ImGui::Begin("Messages"))
                {
                    for (auto& msg : g_Controller.spCurrentProject->projectMessages)
                    {
                        ImGui::TextWrapped("%s", msg.text.c_str());
                        ImGui::Separator();
                    }
                }
                ImGui::End();
            }
        }

        // With a normal device state, draw the scene
        if (g_pDevice->Context().deviceState == DeviceState::Normal && project_has_scene(g_Controller.spCurrentProject.get()))
        {
            validation_enable_messages(true);

            auto spScene = g_Controller.spCurrentProject->spScene;
            LOG_SCOPE(DBG, "\nDraw Current Scene: " << spScene.get());

            window_render(*spScene, appConfig.draw_on_background, [=](const glm::vec2& size, Scene& scene) {
                return g_pDevice->Render_3D(scene, size);
            });

            // Report the errors, regardless
            copy_scene_errors_to_zep(*spScene);

            if (g_pDevice->Context().deviceState == DeviceState::Lost)
            {
                // Device lost, reset.
                // Note: this currently shouldn't happen, and I haven't figured a workaround for it anyway
                // - once the device is lost, only an .exe restart seems to work.
                continue;
            }

            window_targets(*g_Controller.spCurrentProject->spScene);
            validation_enable_messages(false);
        }

        if (g_WindowEnables.profiler)
        {
            if (ImGui::Begin("Profiler", &g_WindowEnables.profiler))
            {
                Zest::Profiler::ShowProfile();
            }
            ImGui::End();
        }

        // Show the editor
        zep_show(zepFocusFlags);
        zepFocusFlags = 0;

        // Rendering
        ImGui::Render();

        ImDrawData* main_draw_data = ImGui::GetDrawData();
        g_pDevice->Context().minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);

        if (!g_pDevice->Context().minimized)
        {
            try
            {
                validation_enable_messages(true);
                LOG_SCOPE(DBG, "Draw IMGUI created data");
                g_pDevice->ImGui_Render(main_draw_data);

                /*
                if (g_Controller.spCurrentProject && project_has_scene(g_Controller.spCurrentProject.get()))
                {
                    g_pDevice->WriteToFile(*g_Controller.spCurrentProject->spScene, fs::path("d:/dev/vklive_renders"));
                }*/
            }
            catch (std::exception& ex)
            {
                if (g_Controller.spCurrentProject && project_has_scene(g_Controller.spCurrentProject.get()))
                {
                    LOG(ERR, "Exception drawing IMGUI! " << ex.what());
                    g_pDevice->DestroyScene(*g_Controller.spCurrentProject->spScene);
                }
            }
        }

        auto& io = ImGui::GetIO();

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        // Present Main Platform Window
        if (!g_pDevice->Context().minimized)
        {
            g_pDevice->Present();
        }

        // We have been once through, no more messages until the user recompiles
        validation_enable_messages(false);
    }

    Zing::audio_destroy();

    quit_thread = true;
    update_thread.join();

    // Cleanup
    zep_destroy();

    save_state();

    config_save(settings_path);

    if (project_has_scene(g_Controller.spCurrentProject.get()))
    {
        g_pDevice->DestroyScene(*g_Controller.spCurrentProject->spScene.get());
    }
    g_pDevice.reset();

    scene_destroy_parser();

    SDL_Quit();

    // python_destroy();

    Zest::Profiler::Finish();

    return 0;
}
