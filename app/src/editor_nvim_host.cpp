#include <app/editor_nvim_host.h>

#include <app/config.h>
#include <app/editor_nvim_renderer.h>
#include <app/editor_nvim_session.h>

#include <vklive/scene.h>

#include <vklive_nvim/input.h>

#include <zest/file/file.h>
#include <zest/imgui/imgui.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <string>

namespace
{

NvimEditorSession& nvim_session()
{
    static NvimEditorSession session;
    return session;
}

NvimImGuiRenderer& nvim_renderer()
{
    static NvimImGuiRenderer renderer;
    return renderer;
}

bool g_windowFocused = false;

bool should_forward_keydown(SDL_Keycode key, SDL_Keymod mods, const std::string& input)
{
    if (input.empty())
    {
        return false;
    }

    const bool hasCommandModifier = (mods & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) != 0;
    if (hasCommandModifier)
    {
        return true;
    }

    if (input.size() == 1 && key >= 32 && key <= 126)
    {
        return false;
    }

    return true;
}

ImGuiWindowFlags nvim_window_flags()
{
    auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (appConfig.transparent_editor && appConfig.draw_on_background)
    {
        flags |= ImGuiWindowFlags_NoBackground;
    }
    return flags;
}

} // namespace

std::vector<std::filesystem::path> nvim_editor_collect_edit_files(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> editFiles;
    if (root.empty() || !std::filesystem::is_directory(root))
    {
        return editFiles;
    }

    auto files = Zest::file_gather_files(root);
    for (const auto& file : files)
    {
        if (scene_is_edit_file(file))
        {
            editFiles.push_back(file);
        }
    }

    return editFiles;
}

void nvim_editor_update_files(const std::filesystem::path& root, bool reset)
{
    auto& session = nvim_session();
    if (reset)
    {
        session.stop();
    }

    session.set_project_root(root);
    session.set_files(nvim_editor_collect_edit_files(root), true);
}

void nvim_editor_show(bool focus)
{
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_FirstUseEver);

    static int focusCount = 4;
    if (focus)
    {
        focusCount = 0;
    }
    if (focusCount++ < 4)
    {
        ImGui::SetNextWindowFocus();
    }

    if (!ImGui::Begin("Neovim", nullptr, nvim_window_flags()))
    {
        g_windowFocused = false;
        ImGui::End();
        return;
    }

    ImVec2 available = ImGui::GetContentRegionAvail();
    available.x = std::max(1.0f, available.x);
    available.y = std::max(1.0f, available.y);

    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("NeovimContainer", available);
    g_windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsItemActive();

    const float cellWidth = std::max(1.0f, ImGui::CalcTextSize("M").x);
    const float cellHeight = std::max(1.0f, ImGui::GetTextLineHeightWithSpacing());
    const auto metrics = nvim_grid_metrics(available, cellWidth, cellHeight);

    auto& session = nvim_session();
    if (session.ensure_started(metrics.columns, metrics.rows))
    {
        session.pump();
        nvim_renderer().draw(session.render_model(), topLeft, metrics);
    }
    else
    {
        ImGui::GetWindowDrawList()->AddRectFilled(topLeft, ImVec2(topLeft.x + available.x, topLeft.y + available.y), IM_COL32(18, 22, 27, 255));
        ImGui::GetWindowDrawList()->AddText(topLeft, IM_COL32(238, 97, 91, 255), "Unable to start Neovim. Check that nvim is on PATH.");
    }

    ImGui::End();
}

void nvim_editor_handle_event(const SDL_Event& event)
{
    if (!g_windowFocused)
    {
        return;
    }

    auto& session = nvim_session();
    if (event.type == SDL_TEXTINPUT)
    {
        session.send_input(event.text.text);
    }
    else if (event.type == SDL_KEYDOWN)
    {
        const auto mods = static_cast<SDL_Keymod>(event.key.keysym.mod);
        const std::string input = vklive_nvim::sdl_key_to_nvim(event.key.keysym.sym, mods);
        if (should_forward_keydown(event.key.keysym.sym, mods, input))
        {
            session.send_input(input);
        }
    }
}

void nvim_editor_destroy()
{
    nvim_session().stop();
}

bool nvim_editor_focused()
{
    return g_windowFocused;
}
