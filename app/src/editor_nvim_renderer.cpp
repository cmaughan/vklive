#include <app/editor_nvim_renderer.h>

#include <algorithm>
#include <cmath>

NvimGridMetrics nvim_grid_metrics(ImVec2 available, float cell_width, float cell_height)
{
    NvimGridMetrics metrics;
    metrics.cell_width = std::max(1.0f, cell_width);
    metrics.cell_height = std::max(1.0f, cell_height);
    metrics.columns = std::max(1, static_cast<int>(std::floor(std::max(0.0f, available.x) / metrics.cell_width)));
    metrics.rows = std::max(1, static_cast<int>(std::floor(std::max(0.0f, available.y) / metrics.cell_height)));
    return metrics;
}

void NvimImGuiRenderer::draw(const vklive_nvim::RenderModel& model, ImVec2 top_left, const NvimGridMetrics& metrics) const
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!drawList)
    {
        return;
    }

    constexpr ImU32 baseBackground = IM_COL32(18, 22, 27, 255);
    constexpr ImU32 activeBackground = IM_COL32(31, 38, 46, 255);
    constexpr ImU32 foreground = IM_COL32(223, 228, 232, 255);

    const ImVec2 bottomRight(
        top_left.x + metrics.columns * metrics.cell_width,
        top_left.y + metrics.rows * metrics.cell_height);
    drawList->AddRectFilled(top_left, bottomRight, baseBackground);

    const int rows = std::min(model.rows(), metrics.rows);
    const int columns = std::min(model.columns(), metrics.columns);
    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const auto& cell = model.cell(column, row);
            if (cell.double_width_continuation)
            {
                continue;
            }

            const ImVec2 cellMin(
                top_left.x + column * metrics.cell_width,
                top_left.y + row * metrics.cell_height);
            const ImVec2 cellMax(
                cellMin.x + metrics.cell_width * (cell.double_width ? 2.0f : 1.0f),
                cellMin.y + metrics.cell_height);

            if (cell.highlight_id != 0)
            {
                drawList->AddRectFilled(cellMin, cellMax, activeBackground);
            }

            if (!cell.text.empty() && cell.text != " ")
            {
                drawList->AddText(cellMin, foreground, cell.text.c_str());
            }
        }
    }
}
