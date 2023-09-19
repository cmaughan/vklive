#include "app/menu.h"
#include "app/window_render.h"

#include <zest/logger/logger.h>
#include <app/python_scripting.h>

void window_render(Scene& scene, bool background, const std::function<ImTextureID(const glm::vec2& size, Scene& scene)>& fnRender)
{
    ImVec2 canvas_size;
    ImVec2 canvas_pos;
    ImDrawList* pDrawList = nullptr;

    if (background)
    {
        pDrawList = ImGui::GetBackgroundDrawList();
    }
    else
    {
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(820, 50), ImGuiCond_FirstUseEver);
        ImGui::Begin("Render");
        pDrawList = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos(); // ImDrawList API uses screen coordinates!
        ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // Resize canvas to what's available
        canvas_size.x = std::max(canvas_size.x, 1.0f);
        canvas_size.y = std::max(canvas_size.y, 1.0f);
        ImGui::InvisibleButton("##dummy", canvas_size);
    }

    auto minRect = pDrawList->GetClipRectMin();
    auto maxRect = pDrawList->GetClipRectMax();
    canvas_pos = minRect;
    auto outputSize = glm::vec2(maxRect.x - minRect.x, maxRect.y - minRect.y);

    bool drawn = false;
    if (scene.valid)
    {
        if (outputSize != scene.lastOutputSize)
        {
            scene.lastOutputSize = outputSize;
            scene.sceneFlags |= SceneFlags::DefaultTargetResize;
            Scene::GlobalFrameCount = 0;
        }

        auto textureId = fnRender(glm::vec2(outputSize.x, outputSize.y), scene);

        scene.sceneFlags &= ~SceneFlags::DefaultTargetResize;

        if (textureId)
        {
            pDrawList->AddImage(textureId,
                ImVec2(canvas_pos.x, canvas_pos.y),
                ImVec2(canvas_pos.x + outputSize.x, canvas_pos.y + outputSize.y));
            drawn = true;
        }
    }

    if (!drawn)
    {
        pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y), 0xFFFFFFFF, "No passes draw to the this buffer...");
    }

    python_tick(pDrawList, glm::vec4(canvas_pos.x, canvas_pos.y, outputSize.x, outputSize.y));

    if (!background)
    {
        ImGui::End();
    }
}
