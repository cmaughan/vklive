#include <vklive_nvim/nvim_ui.h>

#include <vklive_nvim/render_model.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

namespace vklive_nvim
{
namespace
{

constexpr int64_t kMaxHighlightId = std::numeric_limits<std::uint16_t>::max();

const MpackValue::ArrayStorage* try_get_array(const MpackValue& value)
{
    if (value.type() != MpackValue::Array)
    {
        return nullptr;
    }

    return &value.as_array();
}

const std::string* try_get_string(const MpackValue& value)
{
    if (value.type() != MpackValue::String)
    {
        return nullptr;
    }

    return &value.as_str();
}

bool try_get_int(const MpackValue& value, int& out)
{
    if (value.type() != MpackValue::Int && value.type() != MpackValue::UInt)
    {
        return false;
    }

    out = static_cast<int>(value.as_int());
    return true;
}

enum class RedrawEventType
{
    Flush,
    GridClear,
    GridCursorGoto,
    GridLine,
    GridResize,
    GridScroll,
};

struct RedrawDispatchEntry
{
    std::string_view name;
    RedrawEventType type;
};

constexpr std::array<RedrawDispatchEntry, 6> kRedrawDispatch = { {
    { "flush", RedrawEventType::Flush },
    { "grid_clear", RedrawEventType::GridClear },
    { "grid_cursor_goto", RedrawEventType::GridCursorGoto },
    { "grid_line", RedrawEventType::GridLine },
    { "grid_resize", RedrawEventType::GridResize },
    { "grid_scroll", RedrawEventType::GridScroll },
} };

const RedrawDispatchEntry* find_redraw_dispatch(std::string_view name)
{
    const auto it = std::lower_bound(kRedrawDispatch.begin(), kRedrawDispatch.end(), name,
        [](const RedrawDispatchEntry& entry, std::string_view value) {
            return entry.name < value;
        });

    if (it == kRedrawDispatch.end() || it->name != name)
    {
        return nullptr;
    }

    return std::to_address(it);
}

} // namespace

void UiEventHandler::set_render_model(RenderModel* model)
{
    m_model = model;
}

void UiEventHandler::process_redraw(const std::vector<MpackValue>& params)
{
    for (const auto& event : params)
    {
        const auto* eventArray = try_get_array(event);
        if (!eventArray || eventArray->empty())
        {
            continue;
        }

        const auto* name = try_get_string((*eventArray)[0]);
        if (!name)
        {
            continue;
        }

        const auto* dispatch = find_redraw_dispatch(*name);
        if (!dispatch)
        {
            continue;
        }

        if (dispatch->type == RedrawEventType::Flush)
        {
            if (on_flush)
            {
                on_flush();
            }
            continue;
        }

        if (dispatch->type == RedrawEventType::GridClear)
        {
            handle_grid_clear();
            continue;
        }

        for (std::size_t i = 1; i < eventArray->size(); ++i)
        {
            const auto& args = (*eventArray)[i];
            switch (dispatch->type)
            {
            case RedrawEventType::GridResize:
                handle_grid_resize(args);
                break;
            case RedrawEventType::GridLine:
                handle_grid_line(args);
                break;
            case RedrawEventType::GridCursorGoto:
                handle_grid_cursor_goto(args);
                break;
            case RedrawEventType::GridScroll:
                handle_grid_scroll(args);
                break;
            case RedrawEventType::Flush:
            case RedrawEventType::GridClear:
                break;
            }
        }
    }
}

void UiEventHandler::handle_grid_resize(const MpackValue& args)
{
    const auto* argsArray = try_get_array(args);
    if (!argsArray || argsArray->size() < 3)
    {
        return;
    }

    int columns = 0;
    int rows = 0;
    if (!try_get_int((*argsArray)[1], columns) || !try_get_int((*argsArray)[2], rows))
    {
        return;
    }

    if (m_model)
    {
        m_model->resize(columns, rows);
    }

    if (on_grid_resize)
    {
        on_grid_resize(columns, rows);
    }
}

void UiEventHandler::handle_grid_line(const MpackValue& args)
{
    if (!m_model)
    {
        return;
    }

    const auto* argsArray = try_get_array(args);
    if (!argsArray || argsArray->size() < 4)
    {
        return;
    }

    int row = 0;
    int column = 0;
    if (!try_get_int((*argsArray)[1], row) || !try_get_int((*argsArray)[2], column))
    {
        return;
    }

    const auto* cells = try_get_array((*argsArray)[3]);
    if (!cells)
    {
        return;
    }

    std::uint16_t highlightId = 0;
    for (const auto& packedCell : *cells)
    {
        const auto* cellArray = try_get_array(packedCell);
        if (!cellArray || cellArray->empty())
        {
            continue;
        }

        const auto* text = try_get_string((*cellArray)[0]);
        if (!text)
        {
            continue;
        }

        if (cellArray->size() >= 2 && !(*cellArray)[1].is_nil())
        {
            int rawHighlight = 0;
            if (!try_get_int((*cellArray)[1], rawHighlight))
            {
                continue;
            }

            rawHighlight = std::clamp(rawHighlight, 0, static_cast<int>(kMaxHighlightId));
            highlightId = static_cast<std::uint16_t>(rawHighlight);
        }

        int repeat = 1;
        if (cellArray->size() >= 3 && !try_get_int((*cellArray)[2], repeat))
        {
            continue;
        }

        repeat = std::max(1, repeat);
        for (int i = 0; i < repeat; ++i)
        {
            m_model->set_cell(column, row, *text, highlightId, false);
            ++column;
        }
    }
}

void UiEventHandler::handle_grid_cursor_goto(const MpackValue& args)
{
    const auto* argsArray = try_get_array(args);
    if (!argsArray || argsArray->size() < 3)
    {
        return;
    }

    int row = 0;
    int column = 0;
    if (!try_get_int((*argsArray)[1], row) || !try_get_int((*argsArray)[2], column))
    {
        return;
    }

    m_cursorColumn = column;
    m_cursorRow = row;

    if (on_cursor_goto)
    {
        on_cursor_goto(column, row);
    }
}

void UiEventHandler::handle_grid_scroll(const MpackValue& args)
{
    if (!m_model)
    {
        return;
    }

    const auto* argsArray = try_get_array(args);
    if (!argsArray || argsArray->size() < 6)
    {
        return;
    }

    int top = 0;
    int bottom = 0;
    int left = 0;
    int right = 0;
    int rows = 0;
    int columns = 0;
    if (!try_get_int((*argsArray)[1], top) || !try_get_int((*argsArray)[2], bottom) || !try_get_int((*argsArray)[3], left)
        || !try_get_int((*argsArray)[4], right) || !try_get_int((*argsArray)[5], rows))
    {
        return;
    }

    if (argsArray->size() >= 7)
    {
        try_get_int((*argsArray)[6], columns);
    }

    m_model->scroll(top, bottom, left, right, rows, columns);
}

void UiEventHandler::handle_grid_clear()
{
    if (m_model)
    {
        m_model->clear();
    }
}

} // namespace vklive_nvim
