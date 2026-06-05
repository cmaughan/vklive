#include <app/editor_nvim_renderer.h>

#include <cassert>

int main()
{
    const auto metrics = nvim_grid_metrics(ImVec2(805.0f, 402.0f), 8.0f, 16.0f);
    assert(metrics.columns == 100);
    assert(metrics.rows == 25);
    assert(metrics.cell_width == 8.0f);
    assert(metrics.cell_height == 16.0f);

    const auto clamped = nvim_grid_metrics(ImVec2(0.0f, -20.0f), 0.0f, 0.0f);
    assert(clamped.columns == 1);
    assert(clamped.rows == 1);
    assert(clamped.cell_width == 1.0f);
    assert(clamped.cell_height == 1.0f);
}
