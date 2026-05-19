#include "app/window_targets.h"
#include "app/menu.h"

#include <algorithm>

#include <zest/imgui/imgui.h>

#include <vklive/IDevice.h>

extern IDevice* GetDevice();

void window_targets(Scene& scene)
{
    if (!g_WindowEnables.targets)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(820, 50), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Targets", &g_WindowEnables.targets))
    {
        auto pDrawList = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        canvas_size.x = std::max(canvas_size.x, 1.0f);
        canvas_size.y = std::max(canvas_size.y, 1.0f);
        ImGui::InvisibleButton("##dummy", canvas_size);

        auto minRect = pDrawList->GetClipRectMin();
        auto maxRect = pDrawList->GetClipRectMax();
        canvas_pos = minRect;
        canvas_size = ImVec2(maxRect.x - minRect.x, maxRect.y - minRect.y);

        const auto targetViews = scene.valid && GetDevice() ? GetDevice()->TargetViews(scene) : std::vector<RenderTargetView>{};
        if (!targetViews.empty())
        {
            const auto heightPerTile = canvas_size.y / static_cast<float>(targetViews.size());
            const auto fontSize = ImGui::GetFontSize();
            for (const auto& view : targetViews)
            {
                const auto ySize = heightPerTile;
                pDrawList->AddImage(view.textureId,
                    ImVec2(canvas_pos.x, canvas_pos.y),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + ySize - fontSize));
                pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y + ySize - fontSize), 0xFFFFFFFF, view.name.c_str());
                canvas_pos.y += ySize;
            }
        }
        else
        {
            pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y), 0xFFFFFFFF, "No targets...");
        }
    }
    ImGui::End();
}
