#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vklive_nvim
{

struct RenderCell
{
    std::string text = " ";
    std::uint16_t highlight_id = 0;
    bool double_width = false;
    bool double_width_continuation = false;
    bool dirty = true;
};

class RenderModel
{
public:
    void resize(int columns, int rows);
    void clear();

    int columns() const
    {
        return m_columns;
    }

    int rows() const
    {
        return m_rows;
    }

    void set_cell(int column, int row, std::string text, std::uint16_t highlight_id, bool double_width);
    const RenderCell& cell(int column, int row) const;
    void scroll(int top, int bottom, int left, int right, int rows, int columns = 0);

private:
    std::size_t index(int column, int row) const;
    void mark_dirty(std::size_t index);
    static RenderCell blank_cell();

    int m_columns = 0;
    int m_rows = 0;
    std::vector<RenderCell> m_cells;
    RenderCell m_emptyCell;
};

} // namespace vklive_nvim
