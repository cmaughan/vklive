#include "app/menu.h"
#include "app/window_targets.h"

#include <zest/imgui/imgui.h>

#include <vklive/IDevice.h>
#include <vklive/vulkan/vulkan_imgui.h>

#include <zest/logger/logger.h>

using namespace vulkan;

extern IDevice* GetDevice();


void window_targets(Scene& scene)
{
    if (!g_WindowEnables.targets)
    {
        return;
    }

    // TODO: Remove device dependence
    VulkanContext& ctx = (VulkanContext&)GetDevice()->Context();
    auto imgui = imgui_context(ctx);

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(820, 50), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Targets", &g_WindowEnables.targets))
    {
        auto pDrawList = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos(); // ImDrawList API uses screen coordinates!
        ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // Resize canvas to what's available
        canvas_size.x = std::max(canvas_size.x, 1.0f);
        canvas_size.y = std::max(canvas_size.y, 1.0f);
        ImGui::InvisibleButton("##dummy", canvas_size);

        auto minRect = pDrawList->GetClipRectMin();
        auto maxRect = pDrawList->GetClipRectMax();
        canvas_pos = minRect;
        canvas_size = ImVec2(maxRect.x - minRect.x, maxRect.y - minRect.y);

        // TODO: Render these in order in the scene graph file?
        // Add labels
        bool drawn = false;
        if (scene.valid)
        {
            // If we have a final target, and we rendered to it
            auto pVulkanScene = vulkan_scene_get(ctx, scene);
            if (pVulkanScene && pVulkanScene->viewableTargets.size() >= 1)
            {
                auto count = pVulkanScene->viewableTargets.size();
                auto height_per_tile = canvas_size.y / count;

                auto fontSize = ImGui::GetFontSize();
                for (auto& target : pVulkanScene->viewableTargets)
                {
                    // Find the thing we just rendered to
                    auto itrTargetData = pVulkanScene->surfaces.find(target);
                    if (itrTargetData != pVulkanScene->surfaces.end())
                    {
                        auto pSurf = itrTargetData->second;
                        if (pSurf->ImGuiDescriptorSet)
                        {
                            auto ySize = height_per_tile;
                            LOG(DBG, "Showing RT:Target with Descriptor: " << pSurf->ImGuiDescriptorSet);
                            pDrawList->AddImage((ImTextureID)pSurf->ImGuiDescriptorSet,
                                ImVec2(canvas_pos.x, canvas_pos.y),
                                ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + ySize - fontSize));
                            pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y + ySize - fontSize), 0xFFFFFFFF, pSurf->debugName.c_str());
                            canvas_pos.y += ySize;
                        }
                        else
                        {
                            // This is the target view, some descriptors are missing
                            // LOG(DBG, "No descriptor?");
                        }
                        drawn = true;
                    }
                }
            }
        }

        if (!drawn)
        {
            pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y), 0xFFFFFFFF, "No targets...");
        }
    }

    ImGui::End();
}
