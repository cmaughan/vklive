#pragma once

#include <vklive_nvim/nvim_rpc.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace vklive_nvim
{

class RenderModel;
class HighlightTable;

class UiEventHandler
{
public:
    void set_render_model(RenderModel* model);
    void set_highlights(HighlightTable* highlights);
    void process_redraw(const std::vector<MpackValue>& params);

    int cursor_column() const
    {
        return m_cursorColumn;
    }

    int cursor_row() const
    {
        return m_cursorRow;
    }

    std::function<void()> on_flush;
    std::function<void(int, int)> on_grid_resize;
    std::function<void(int, int)> on_cursor_goto;

private:
    void handle_grid_resize(const MpackValue& args);
    void handle_grid_line(const MpackValue& args);
    void handle_grid_cursor_goto(const MpackValue& args);
    void handle_grid_scroll(const MpackValue& args);
    void handle_grid_clear();
    void handle_default_colors_set(const MpackValue& args);
    void handle_hl_attr_define(const MpackValue& args);

    RenderModel* m_model = nullptr;
    HighlightTable* m_highlights = nullptr;
    int m_cursorColumn = 0;
    int m_cursorRow = 0;
};

} // namespace vklive_nvim
