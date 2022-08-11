#include <atomic>
#include <mutex>
#include <thread>

#include <clipp.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl.h>

#include <vklive/file/file.h>
#include <vklive/time/timer.h>
#include <vklive/file/runtree.h>
#include <vklive/logger/logger.h>
#include <vklive/threadpool/threadpool.h>

#include <vklive/scene.h>
#include <vklive/IDevice.h>

#include <vklive/validation.h>

#include <vklive/audio/audio.h>

#include <config_app.h>

#include <app/config.h>
#include <app/controller.h>
#include <app/editor.h>
#include <app/menu.h>
#include <app/project.h>

#ifdef _WIN32
// For console
#include <windows.h>
#endif

Logger vklogger{ false, LT::DBG };
bool Log::disabled = false;

Controller controller;
using namespace clipp;

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

bool read_command_line(int argc, char** argv, int& exitCode)
{
    auto cli = group(opt_value("viewports", appConfig.viewports));
    if (argc != 0)
    {
        parse(argc, argv, cli);
    }
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

int main(int argc, char** argv)
{
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
    runtree_init(SDL_GetBasePath(), VKLIVE_ROOT);

    // Get the settings
    auto settings_path = file_init_settings("VkLive", runtree_find_path("settings.toml"), fs::path("settings") / "settings.toml");
    config_load(settings_path);

    auto imSettingsPath = file_init_settings("VkLive", runtree_find_path("imgui.ini"), fs::path("settings") / "imgui.ini").string();

    // Set the audio config from the loaded config. We copy it back when closing the app
    Audio::GetAudioContext().audioAnalysisSettings = appConfig.audioAnalysisSettings;
    Audio::GetAudioContext().audioDeviceSettings = appConfig.audioDeviceSettings;

    // Project
    project_startup();

    // SDL (window management/input)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    timer_restart(globalTimer);

    // Main device 
    g_pDevice = vulkan::create_vulkan_device(init_sdl_window(), imSettingsPath, appConfig.viewports);

    Audio::audio_init(nullptr);

    // This update thread generates a new scene, then returns it in a queue ready for 'swapping' with the existing one
    // if it is valid
    auto spQueue = std::make_shared<moodycamel::ConcurrentQueue<std::shared_ptr<Project>>>();
    controller.spProjectQueue = std::make_shared<moodycamel::ConcurrentQueue<std::shared_ptr<Project>>>();

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
            if (!controller.spProjectQueue->try_dequeue(spProject))
            {
                // Sleep
                std::this_thread::sleep_for(wakeUpDelta);
                continue;
            }

            LOG(INFO, "Scene initing");

            spProject->spScene = scene_build(spProject->rootPath);

            // May not be valid, but sent anyway
            g_pDevice->InitScene(*spProject->spScene);

            spQueue->enqueue(spProject);
        }
    });

    // Startup, load the default project
    auto project = project_load(appConfig.project_root);
    controller.spProjectQueue->enqueue(project);

    // Main loop
    bool done = false;
    while (!done)
    {
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
        }

        // Attempt to recover a lost device: this does not work!
        // It is a hard problem to recover from some combinatoric errors of state.
        // We now rely on the validation layer to catch everything and ensure if it does, we don't draw.
        // So far, this protects from lost devices.
        // We may some day get this to work; but for now, once the device is gone, there seems to be no way to recover (
        // creating the device always fails)
        if (g_pDevice->Context().deviceState == DeviceState::Lost)
        {
            // Try to restart
            if (project_has_scene(controller.spCurrentProject.get()))
            {
                g_pDevice->DestroyScene(*controller.spCurrentProject->spScene.get());
            }
            g_pDevice = vulkan::create_vulkan_device(init_sdl_window(), imSettingsPath, appConfig.viewports);

            // Remember we were lost
            g_pDevice->Context().deviceState = DeviceState::WasLost;
        }
        g_pDevice->ValidateSwapChain();

        // Start the Dear ImGui frame
        ImGui_ImplSDL2_NewFrame(g_pDevice->Context().window);
        ImGui::NewFrame();

        menu_show();

        //ImGui::ShowDemoWindow();

        static bool update = false;
        static bool z_init = false;
        static bool focusEditor = false;
        if (!z_init)
        {
            // Called once the fonts/device is guaranteed setup
            zep_init(runtree_path(), Zep::NVec2f(1.0f, 1.0f), [&](Zep::ZepBuffer& buffer, const Zep::GlyphIterator& itr) {
                // Flash the buffer
                Zep::FlashType type = Zep::FlashType::Flash;
                buffer.BeginFlash(0.75f, type, Zep::GlyphRange(buffer.Begin(), buffer.End()));

                // Save the buffers
                if (buffer.HasFileFlags(Zep::FileFlags::Dirty))
                {
                    int64_t sz;
                    buffer.Save(sz);
                }

                if (project_scene_valid(controller.spCurrentProject.get()))
                {
                    auto updateFile = [](auto f) {
                        auto shader = zep_get_editor().FindFileBuffer(f.string());
                        if (shader && shader->HasFileFlags(Zep::FileFlags::Dirty))
                        {
                            int64_t sz;
                            shader->Save(sz);
                        }
                    };

                    auto spScene = controller.spCurrentProject->spScene;
                    std::for_each(spScene->shaders.begin(), spScene->shaders.end(), [=](auto f) { updateFile(f.first); });
                    std::for_each(spScene->headers.begin(), spScene->headers.end(), [=](auto f) { updateFile(f); });
                }

                // Make a new project from the existing and queue it for loading
                auto spProject = std::make_shared<Project>();
                spProject->rootPath = controller.spCurrentProject->rootPath;
                spProject->temporary = controller.spCurrentProject->temporary;
                spProject->modified = true;
                controller.spProjectQueue->enqueue(spProject);
            });


            z_init = true;
        }

        std::shared_ptr<Project> spNewProject;
        if (spQueue->try_dequeue(spNewProject))
        {
            validation_enable_messages(true);

            zep_clear_all_messages();

            bool switchProject = (!controller.spCurrentProject || !fs::equivalent(controller.spCurrentProject->rootPath, spNewProject->rootPath));

            // Do a 'pre-render'.  If this fails, then we will catch errors that might only happen during scene prepare
            if (project_scene_valid(spNewProject.get()))
            {
                g_pDevice->ImGui_Render_3D(*spNewProject->spScene, appConfig.draw_on_background);
            }

            // If the new project is still valid and has a working scene, swap
            // We still receive invalid projects, because we want to gather error info
            if (project_scene_valid(spNewProject.get()))
            {
                g_pDevice->WaitIdle();

                if (project_scene_valid(controller.spCurrentProject.get()))
                {
                    g_pDevice->DestroyScene(*controller.spCurrentProject->spScene);

                    // Copy over the old info, if appropriate - this is temporary fix for cleaner solution later.
                    auto spNewScene = spNewProject->spScene;
                    for (auto& [name, pass] : spNewScene->passes)
                    {
                        auto itrOrig = controller.spCurrentProject->spScene->passes.find(name);
                        if (itrOrig != controller.spCurrentProject->spScene->passes.end())
                        {
                            pass->camera = itrOrig->second->camera;
                        }
                    }
                }

                // Copy the new one
                controller.spCurrentProject = spNewProject;

                // Back to a normal rendering state (see device lost comments)
                g_pDevice->Context().deviceState = DeviceState::Normal;
            }
            else
            {
                // We have switched projects, so even if the project doesn't load, we have to be in the mode to show
                // its files.  There will be no render, but the user can fix stuff
                if (switchProject)
                {
                    controller.spCurrentProject = spNewProject;
                    zep_update_files(controller.spCurrentProject->rootPath, switchProject);
                }
            }

            zep_update_files(controller.spCurrentProject->rootPath, switchProject);

            // Reset the errors
            controller.spCurrentProject->projectMessages.clear();

            // Report the errors, regardless
            for (auto& err : spNewProject->spScene->errors)
            {
                if (!err.path.empty())
                {
                    zep_add_file_message(err);
                }
            }

            focusEditor = true;
        }

        if (controller.spCurrentProject)
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
                    if (controller.spCurrentProject->projectMessages.size() < 5)
                    {
                        controller.spCurrentProject->projectMessages.push_back(msg);
                    }
                }
            }

            if (!controller.spCurrentProject->projectMessages.empty())
            {
                if (ImGui::Begin("Messages"))
                {
                    for (auto& msg : controller.spCurrentProject->projectMessages)
                    {
                        ImGui::TextWrapped("%s", msg.text.c_str());
                        ImGui::Separator();
                    }
                }
                ImGui::End();
            }
        }

        // With a normal device state, draw the scene
        if (g_pDevice->Context().deviceState == DeviceState::Normal && project_has_scene(controller.spCurrentProject.get()))
        {
            // Scene may not be valid, but we want to draw the window
            g_pDevice->ImGui_Render_3D(*controller.spCurrentProject->spScene, appConfig.draw_on_background);
            if (g_pDevice->Context().deviceState == DeviceState::Lost)
            {
                // Device lost, reset.
                continue;
            }
            globalFrameCount++;
        }

        // Just show it
        zep_show(focusEditor);
        focusEditor = false;

        // Rendering
        ImGui::Render();

        ImDrawData* main_draw_data = ImGui::GetDrawData();
        g_pDevice->Context().minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);

        if (!g_pDevice->Context().minimized)
        {
            g_pDevice->ImGui_Render(main_draw_data);
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

    quit_thread = true;
    update_thread.join();

    // Cleanup
    zep_destroy();

    // If not temporary, remember what we last looked at
    if (controller.spCurrentProject && !controller.spCurrentProject->temporary)
    {
        appConfig.project_root = controller.spCurrentProject->rootPath;
    }

    // Window Config
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

    config_save(settings_path);

    if (project_has_scene(controller.spCurrentProject.get()))
    {
        g_pDevice->DestroyScene(*controller.spCurrentProject->spScene.get());
    }
    g_pDevice.reset();

    Audio::audio_destroy();

    scene_destroy_parser();

    SDL_Quit();

    return 0;
}
