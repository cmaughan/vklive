#pragma once

#include <zest/imgui/imgui.h>

#include <vklive_nvim/render_model.h>

struct NvimGridMetrics
{
    int columns = 1;
    int rows = 1;
    float cell_width = 1.0f;
    float cell_height = 1.0f;
};

NvimGridMetrics nvim_grid_metrics(ImVec2 available, float cell_width, float cell_height);

class NvimImGuiRenderer
{
public:
    void draw(const vklive_nvim::RenderModel& model, ImVec2 top_left, const NvimGridMetrics& metrics) const;
};
