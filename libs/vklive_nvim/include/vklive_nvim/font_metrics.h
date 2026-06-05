#pragma once

namespace vklive_nvim
{

struct FontMetrics
{
    int cell_width; // Monospace advance width
    int cell_height; // Line height
    int ascender; // Pixels above baseline
    int descender; // Pixels below baseline (positive = down)
};

} // namespace vklive_nvim
