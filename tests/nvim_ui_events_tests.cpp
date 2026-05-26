#include <vklive_nvim/nvim_rpc.h>
#include <vklive_nvim/nvim_ui.h>
#include <vklive_nvim/render_model.h>

#include <cassert>
#include <string>
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

} // namespace

int main()
{
    vklive_nvim::RenderModel model;
    vklive_nvim::UiEventHandler handler;
    handler.set_render_model(&model);

    int flushes = 0;
    handler.on_flush = [&flushes]() {
        ++flushes;
    };

    handler.process_redraw({
        array({ str("grid_resize"), array({ integer(1), integer(4), integer(2) }) }),
        array({ str("grid_line"), array({ integer(1), integer(0), integer(0), array({
            array({ str("A"), integer(3) }),
            array({ str("B"), vklive_nvim::NvimRpc::make_nil(), integer(2) }),
        }) }) }),
        array({ str("grid_cursor_goto"), array({ integer(1), integer(0), integer(2) }) }),
        array({ str("flush") }),
    });

    assert(model.columns() == 4);
    assert(model.rows() == 2);
    assert(model.cell(0, 0).text == std::string("A"));
    assert(model.cell(0, 0).highlight_id == 3);
    assert(model.cell(1, 0).text == std::string("B"));
    assert(model.cell(1, 0).highlight_id == 3);
    assert(model.cell(2, 0).text == std::string("B"));
    assert(handler.cursor_column() == 2);
    assert(handler.cursor_row() == 0);
    assert(flushes == 1);
}
