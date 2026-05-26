#include <vklive_nvim/render_model.h>

#include <cassert>
#include <string>

int main()
{
    vklive_nvim::RenderModel model;
    model.resize(4, 2);
    assert(model.columns() == 4);
    assert(model.rows() == 2);

    model.set_cell(1, 0, "A", 7, false);
    const auto& cell = model.cell(1, 0);
    assert(cell.text == std::string("A"));
    assert(cell.highlight_id == 7);
    assert(!cell.double_width);

    model.set_cell(2, 0, "W", 8, true);
    assert(model.cell(2, 0).double_width);
    assert(model.cell(3, 0).double_width_continuation);

    model.clear();
    assert(model.cell(1, 0).text == std::string(" "));
    assert(model.cell(1, 0).highlight_id == 0);
}
