#include <vklive_nvim/nvim_ui.h>

#include <vklive_nvim/highlight.h>
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
    DefaultColorsSet,
    Flush,
    GridClear,
    GridCursorGoto,
    GridLine,
    GridResize,
    GridScroll,
    HlAttrDefine,
};

struct RedrawDispatchEntry
{
    std::string_view name;
    RedrawEventType type;
};

constexpr std::array<RedrawDispatchEntry, 8> kRedrawDispatch = { {
    { "default_colors_set", RedrawEventType::DefaultColorsSet },
    { "flush", RedrawEventType::Flush },
    { "grid_clear", RedrawEventType::GridClear },
    { "grid_cursor_goto", RedrawEventType::GridCursorGoto },
    { "grid_line", RedrawEventType::GridLine },
    { "grid_resize", RedrawEventType::GridResize },
    { "grid_scroll", RedrawEventType::GridScroll },
    { "hl_attr_define", RedrawEventType::HlAttrDefine },
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

void UiEventHandler::set_highlights(HighlightTable* highlights)
{
    m_highlights = highlights;
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
            case RedrawEventType::DefaultColorsSet:
                handle_default_colors_set(args);
                break;
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
            case RedrawEventType::HlAttrDefine:
                handle_hl_attr_define(args);
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

void UiEventHandler::handle_default_colors_set(const MpackValue& args)
{
    if (!m_highlights)
    {
        return;
    }

    const auto* argsArray = try_get_array(args);
    if (!argsArray || argsArray->size() < 3)
    {
        return;
    }

    int foreground = 0;
    int background = 0;
    int special = 0;
    if (!try_get_int((*argsArray)[0], foreground) || !try_get_int((*argsArray)[1], background) || !try_get_int((*argsArray)[2], special))
    {
        return;
    }

    m_highlights->set_default_fg(color_from_rgb(static_cast<std::uint32_t>(foreground)));
    m_highlights->set_default_bg(color_from_rgb(static_cast<std::uint32_t>(background)));
    m_highlights->set_default_sp(color_from_rgb(static_cast<std::uint32_t>(special)));
}

void UiEventHandler::handle_hl_attr_define(const MpackValue& args)
{
    if (!m_highlights)
    {
        return;
    }

    const auto* argsArray = try_get_array(args);
    if (!argsArray || argsArray->size() < 2)
    {
        return;
    }

    int rawHighlightId = 0;
    if (!try_get_int((*argsArray)[0], rawHighlightId))
    {
        return;
    }

    rawHighlightId = std::clamp(rawHighlightId, 0, static_cast<int>(kMaxHighlightId));
    HlAttr attr;
    const auto& attrs = (*argsArray)[1];
    if (attrs.type() == MpackValue::Map)
    {
        for (const auto& [key, value] : attrs.as_map())
        {
            const auto* name = try_get_string(key);
            if (!name)
            {
                continue;
            }

            if (*name == "foreground")
            {
                int color = 0;
                if (try_get_int(value, color))
                {
                    attr.fg = color_from_rgb(static_cast<std::uint32_t>(color));
                    attr.has_fg = true;
                }
            }
            else if (*name == "background")
            {
                int color = 0;
                if (try_get_int(value, color))
                {
                    attr.bg = color_from_rgb(static_cast<std::uint32_t>(color));
                    attr.has_bg = true;
                }
            }
            else if (*name == "special")
            {
                int color = 0;
                if (try_get_int(value, color))
                {
                    attr.sp = color_from_rgb(static_cast<std::uint32_t>(color));
                    attr.has_sp = true;
                }
            }
            else if (*name == "bold" && value.type() == MpackValue::Bool)
            {
                attr.bold = value.as_bool();
            }
            else if (*name == "italic" && value.type() == MpackValue::Bool)
            {
                attr.italic = value.as_bool();
            }
            else if (*name == "underline" && value.type() == MpackValue::Bool)
            {
                attr.underline = value.as_bool();
            }
            else if (*name == "undercurl" && value.type() == MpackValue::Bool)
            {
                attr.undercurl = value.as_bool();
            }
            else if (*name == "strikethrough" && value.type() == MpackValue::Bool)
            {
                attr.strikethrough = value.as_bool();
            }
            else if (*name == "reverse" && value.type() == MpackValue::Bool)
            {
                attr.reverse = value.as_bool();
            }
        }
    }

    m_highlights->set(static_cast<std::uint16_t>(rawHighlightId), attr);
}

} // namespace vklive_nvim
