#include <app/editor_nvim_renderer.h>

int main()
{
    const auto metrics = nvim_grid_metrics(ImVec2(805.0f, 402.0f), 8.0f, 16.0f);
    if (metrics.columns != 100 || metrics.rows != 25 || metrics.cell_width != 8.0f || metrics.cell_height != 16.0f)
    {
        return 1;
    }

    const auto clamped = nvim_grid_metrics(ImVec2(0.0f, -20.0f), 0.0f, 0.0f);
    if (clamped.columns != 1 || clamped.rows != 1 || clamped.cell_width != 1.0f || clamped.cell_height != 1.0f)
    {
        return 1;
    }

    NvimImGuiRenderer normalDpi;
    NvimImGuiRenderer highDpi;
    if (!normalDpi.ensure_initialized(96.0f) || !highDpi.ensure_initialized(192.0f))
    {
        return 1;
    }
    if (normalDpi.cell_size().x != highDpi.cell_size().x || normalDpi.cell_size().y != highDpi.cell_size().y)
    {
        return 1;
    }

    vklive_nvim::FontMetrics fontMetrics;
    fontMetrics.cell_width = 8;
    fontMetrics.cell_height = 16;
    fontMetrics.ascender = 12;
    fontMetrics.descender = 4;

    vklive_nvim::AtlasRegion glyph;
    glyph.bitmap_bearing = { 1, 9 };
    glyph.bitmap_size = { 7, 10 };

    const ImVec2 origin = nvim_glyph_origin(ImVec2(20.0f, 30.0f), fontMetrics, glyph);
    if (origin.x != 21.0f || origin.y != 33.0f)
    {
        return 1;
    }

    return 0;
}
