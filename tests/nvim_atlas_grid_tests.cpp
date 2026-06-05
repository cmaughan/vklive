#include <vklive_nvim/atlas_grid.h>

#include <cassert>
#include <string>

namespace
{

class FakeAtlas final : public vklive_nvim::IGlyphAtlas
{
public:
    vklive_nvim::AtlasRegion resolve_cluster(const std::string& text, bool is_bold, bool is_italic) override
    {
        last_text = text;
        last_bold = is_bold;
        last_italic = is_italic;
        resolve_count++;

        vklive_nvim::AtlasRegion region;
        region.uv = { 0.25f, 0.5f, 0.375f, 0.625f };
        region.bitmap_bearing = { 1, 8 };
        region.bitmap_size = { 8, 10 };
        region.advance_px = 9;
        return region;
    }

    int ligature_cell_span(const std::string&, bool, bool) override
    {
        return 1;
    }

    bool atlas_dirty() const override
    {
        return false;
    }

    bool consume_atlas_reset() override
    {
        return false;
    }

    void clear_atlas_dirty() override
    {
    }

    const uint8_t* atlas_data() const override
    {
        return nullptr;
    }

    int atlas_width() const override
    {
        return 2048;
    }

    int atlas_height() const override
    {
        return 2048;
    }

    vklive_nvim::AtlasDirtyRect atlas_dirty_rect() const override
    {
        return {};
    }

    std::string last_text;
    bool last_bold = false;
    bool last_italic = false;
    int resolve_count = 0;
};

} // namespace

int main()
{
    vklive_nvim::RenderModel model;
    model.resize(1, 1);
    model.set_cell(0, 0, "A", 7, false);

    vklive_nvim::HighlightTable highlights;
    vklive_nvim::HlAttr attr;
    attr.fg = vklive_nvim::Color(0.8f, 0.7f, 0.6f, 1.0f);
    attr.bg = vklive_nvim::Color(0.1f, 0.2f, 0.3f, 1.0f);
    attr.has_fg = true;
    attr.has_bg = true;
    attr.bold = true;
    attr.italic = true;
    highlights.set(7, attr);

    FakeAtlas atlas;
    const auto cells = vklive_nvim::build_atlas_grid_cells(model, atlas, highlights);

    assert(cells.size() == 1);
    assert(atlas.resolve_count == 1);
    assert(atlas.last_text == "A");
    assert(atlas.last_bold);
    assert(atlas.last_italic);

    const auto& cell = cells[0];
    assert(cell.column == 0);
    assert(cell.row == 0);
    assert(cell.cell_span == 1);
    assert(cell.glyph.bitmap_size.x == 8);
    assert((cell.style_flags & vklive_nvim::STYLE_FLAG_BOLD) != 0);
    assert((cell.style_flags & vklive_nvim::STYLE_FLAG_ITALIC) != 0);
    assert(cell.foreground.r == attr.fg.r);
    assert(cell.background.b == attr.bg.b);

    return 0;
}
