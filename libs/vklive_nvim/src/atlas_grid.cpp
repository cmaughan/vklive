#include <vklive_nvim/atlas_grid.h>

namespace vklive_nvim
{

std::vector<AtlasGridCell> build_atlas_grid_cells(
    const RenderModel& model,
    IGlyphAtlas& atlas,
    const HighlightTable& highlights)
{
    std::vector<AtlasGridCell> cells;
    cells.reserve(static_cast<std::size_t>(model.columns()) * static_cast<std::size_t>(model.rows()));

    for (int row = 0; row < model.rows(); ++row)
    {
        for (int column = 0; column < model.columns(); ++column)
        {
            const RenderCell& source = model.cell(column, row);
            const HlAttr& attr = highlights.get(source.highlight_id);

            AtlasGridCell cell;
            cell.column = column;
            cell.row = row;
            cell.cell_span = source.double_width ? 2 : 1;
            highlights.resolve(attr, cell.foreground, cell.background, &cell.special);
            if (source.highlight_id != 0 && !highlights.contains(source.highlight_id))
            {
                cell.background = Color(0.12f, 0.15f, 0.18f, 1.0f);
            }
            cell.style_flags = attr.style_flags();

            if (!source.double_width_continuation)
            {
                const bool is_bold = (cell.style_flags & STYLE_FLAG_BOLD) != 0;
                const bool is_italic = (cell.style_flags & STYLE_FLAG_ITALIC) != 0;
                cell.glyph = atlas.resolve_cluster(source.text, is_bold, is_italic);
                if (cell.glyph.is_color)
                {
                    cell.style_flags |= STYLE_FLAG_COLOR_GLYPH;
                }
            }

            cells.push_back(cell);
        }
    }

    return cells;
}

} // namespace vklive_nvim
