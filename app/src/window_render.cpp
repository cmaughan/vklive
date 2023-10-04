#include "app/window_render.h"
#include "app/menu.h"
#include "app/editor.h"

#include <vklive/python_scripting.h>
#include <vklive/IDevice.h>

#include <zest/logger/logger.h>

void window_render(IDevice* pDevice, Scene& scene, bool background, const std::function<RenderOutput(const glm::vec2& size, Scene& scene)>& fnRender)
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

        auto renderOutput = fnRender(glm::vec2(outputSize.x, outputSize.y), scene);
        if (renderOutput.pSurface)
        {
            scene.sceneFlags &= ~SceneFlags::DefaultTargetResize;

            auto border = glm::vec2(0.0f);
            auto currentSurfaceSize = renderOutput.pSurface->currentSize;
            if (currentSurfaceSize.x < outputSize.x)
            {
                border.x = (outputSize.x - currentSurfaceSize.x) * 0.5f;
            }

            if (currentSurfaceSize.y < outputSize.y)
            {
                border.y = (outputSize.y - currentSurfaceSize.y) * 0.5f;
            }

            const auto frameSize = 2.0f;
            auto topLeft = ImVec2(canvas_pos.x + border.x - frameSize, canvas_pos.y + border.y - frameSize);
            auto bottomRight = ImVec2(canvas_pos.x + currentSurfaceSize.x + border.x + frameSize, canvas_pos.y + currentSurfaceSize.y + border.y + frameSize);

            if (border.x > 0 || border.y > 0)
            {
                pDrawList->AddRectFilled(topLeft, bottomRight, 0xFF888888);
            }

            topLeft.x += frameSize;
            topLeft.y += frameSize;
            bottomRight.x -= frameSize;
            bottomRight.y -= frameSize;

            if (renderOutput.textureId)
            {
                pDrawList->AddImage(renderOutput.textureId, topLeft, bottomRight);

                scene.targetViewport = glm::vec4(topLeft.x, topLeft.y, std::min(bottomRight.x, maxRect.x), std::min(bottomRight.y, maxRect.y));

                drawn = true;
            }

            for (auto& p : scene.post_2d)
            {
                auto itr = scene.scripts.find(p);
                if (itr != scene.scripts.end())
                {
                    python_run_2d(*itr->second, pDevice, scene, pDrawList, glm::vec4(canvas_pos.x + border.x, canvas_pos.y + border.y, currentSurfaceSize.x, currentSurfaceSize.y));
                }
                drawn = true;
            }
        }
    }

    if (!drawn)
    {
        pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y), 0xFFFFFFFF, "No passes draw to the this buffer...");
    }

    if (!background)
    {
        ImGui::End();
    }
}
