#pragma once

#include <vklive_nvim/glyph_atlas.h>
#include <vklive_nvim/highlight.h>
#include <vklive_nvim/render_model.h>
#include <vklive_nvim/types.h>

#include <vector>

namespace vklive_nvim
{

struct AtlasGridCell
{
    int column = 0;
    int row = 0;
    int cell_span = 1;
    Color foreground = Color(1.0f, 1.0f, 1.0f, 1.0f);
    Color background = Color(0.0f, 0.0f, 0.0f, 1.0f);
    Color special = Color(1.0f, 1.0f, 1.0f, 1.0f);
    AtlasRegion glyph = {};
    uint32_t style_flags = 0;
};

std::vector<AtlasGridCell> build_atlas_grid_cells(
    const RenderModel& model,
    IGlyphAtlas& atlas,
    const HighlightTable& highlights);

} // namespace vklive_nvim
