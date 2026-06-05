#include <vklive_nvim/nvim_rpc.h>
#include <vklive_nvim/nvim_ui.h>
#include <vklive_nvim/highlight.h>
#include <vklive_nvim/render_model.h>

#include <string>
#include <utility>
#include <vector>

namespace
{

vklive_nvim::MpackValue array(std::vector<vklive_nvim::MpackValue> values)
{
    return vklive_nvim::NvimRpc::make_array(std::move(values));
}

vklive_nvim::MpackValue str(const std::string& value)
{
    return vklive_nvim::NvimRpc::make_str(value);
}

vklive_nvim::MpackValue integer(int value)
{
    return vklive_nvim::NvimRpc::make_int(value);
}

vklive_nvim::MpackValue boolean(bool value)
{
    return vklive_nvim::NvimRpc::make_bool(value);
}

vklive_nvim::MpackValue map(std::vector<std::pair<vklive_nvim::MpackValue, vklive_nvim::MpackValue>> values)
{
    return vklive_nvim::NvimRpc::make_map(std::move(values));
}

} // namespace

int main()
{
    vklive_nvim::RenderModel model;
    vklive_nvim::HighlightTable highlights;
    vklive_nvim::UiEventHandler handler;
    handler.set_render_model(&model);
    handler.set_highlights(&highlights);

    int flushes = 0;
    handler.on_flush = [&flushes]() {
        ++flushes;
    };

    handler.process_redraw({
        array({ str("default_colors_set"), array({ integer(0xD4D4D4), integer(0x101820), integer(0x55AAFF) }) }),
        array({ str("hl_attr_define"), array({ integer(7), map({
            { str("foreground"), integer(0x123456) },
            { str("background"), integer(0x654321) },
            { str("special"), integer(0xABCDEF) },
            { str("bold"), boolean(true) },
            { str("italic"), boolean(true) },
        }) }) }),
        array({ str("grid_resize"), array({ integer(1), integer(4), integer(2) }) }),
        array({ str("grid_line"), array({ integer(1), integer(0), integer(0), array({
            array({ str("A"), integer(7) }),
            array({ str("B"), vklive_nvim::NvimRpc::make_nil(), integer(2) }),
        }) }) }),
        array({ str("grid_cursor_goto"), array({ integer(1), integer(0), integer(2) }) }),
        array({ str("flush") }),
    });

    if (model.columns() != 4 || model.rows() != 2)
    {
        return 1;
    }
    if (model.cell(0, 0).text != std::string("A") || model.cell(0, 0).highlight_id != 7)
    {
        return 1;
    }
    if (model.cell(1, 0).text != std::string("B") || model.cell(1, 0).highlight_id != 7)
    {
        return 1;
    }
    if (model.cell(2, 0).text != std::string("B"))
    {
        return 1;
    }
    if (handler.cursor_column() != 2 || handler.cursor_row() != 0 || flushes != 1)
    {
        return 1;
    }

    const auto& attr = highlights.get(7);
    if (!attr.has_fg || !attr.has_bg || !attr.has_sp || !attr.bold || !attr.italic)
    {
        return 1;
    }
    if (highlights.default_bg() != vklive_nvim::color_from_rgb(0x101820))
    {
        return 1;
    }

    return 0;
}
