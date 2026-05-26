#include <vklive_nvim/render_model.h>

#include <algorithm>

namespace vklive_nvim
{
namespace
{

constexpr int kMaxGridDimension = 10000;

} // namespace

void RenderModel::resize(int columns, int rows)
{
    columns = std::clamp(columns, 0, kMaxGridDimension);
    rows = std::clamp(rows, 0, kMaxGridDimension);

    if (columns == m_columns && rows == m_rows)
    {
        return;
    }

    std::vector<RenderCell> next(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), blank_cell());
    const int copyColumns = std::min(columns, m_columns);
    const int copyRows = std::min(rows, m_rows);

    for (int row = 0; row < copyRows; ++row)
    {
        for (int column = 0; column < copyColumns; ++column)
        {
            next[static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(column)] = m_cells[index(column, row)];
        }
    }

    m_columns = columns;
    m_rows = rows;
    m_cells = std::move(next);
}

void RenderModel::clear()
{
    std::fill(m_cells.begin(), m_cells.end(), blank_cell());
}

void RenderModel::set_cell(int column, int row, std::string text, std::uint16_t highlight_id, bool double_width)
{
    if (column < 0 || column >= m_columns || row < 0 || row >= m_rows)
    {
        return;
    }

    const std::size_t cellIndex = index(column, row);
    if (column > 0)
    {
        auto& previous = m_cells[cellIndex - 1];
        if (previous.double_width || m_cells[cellIndex].double_width_continuation)
        {
            previous = blank_cell();
            mark_dirty(cellIndex - 1);
        }
    }

    if (column + 1 < m_columns && m_cells[cellIndex + 1].double_width_continuation)
    {
        m_cells[cellIndex + 1] = blank_cell();
        mark_dirty(cellIndex + 1);
    }

    auto& cell = m_cells[cellIndex];
    cell.text = std::move(text);
    cell.highlight_id = highlight_id;
    cell.double_width = double_width;
    cell.double_width_continuation = false;
    mark_dirty(cellIndex);

    if (double_width && column + 1 < m_columns)
    {
        auto& continuation = m_cells[cellIndex + 1];
        continuation = blank_cell();
        continuation.highlight_id = highlight_id;
        continuation.double_width_continuation = true;
        mark_dirty(cellIndex + 1);
    }
}

const RenderCell& RenderModel::cell(int column, int row) const
{
    if (column < 0 || column >= m_columns || row < 0 || row >= m_rows)
    {
        return m_emptyCell;
    }

    return m_cells[index(column, row)];
}

void RenderModel::scroll(int top, int bottom, int left, int right, int rows, int columns)
{
    if (rows == 0 && columns == 0)
    {
        return;
    }

    if (top < 0 || top >= bottom || bottom > m_rows || left < 0 || left >= right || right > m_columns)
    {
        return;
    }

    const int regionRows = bottom - top;
    const int regionColumns = right - left;
    rows = std::clamp(rows, -regionRows, regionRows);
    columns = std::clamp(columns, -regionColumns, regionColumns);

    auto sourceOrBlank = [&](int column, int row) {
        const int sourceColumn = column + columns;
        const int sourceRow = row + rows;
        if (sourceColumn < left || sourceColumn >= right || sourceRow < top || sourceRow >= bottom)
        {
            return blank_cell();
        }
        return m_cells[index(sourceColumn, sourceRow)];
    };

    std::vector<RenderCell> region(static_cast<std::size_t>(regionRows) * static_cast<std::size_t>(regionColumns), blank_cell());
    for (int row = top; row < bottom; ++row)
    {
        for (int column = left; column < right; ++column)
        {
            region[static_cast<std::size_t>(row - top) * static_cast<std::size_t>(regionColumns) + static_cast<std::size_t>(column - left)] = sourceOrBlank(column, row);
        }
    }

    for (int row = top; row < bottom; ++row)
    {
        for (int column = left; column < right; ++column)
        {
            const auto sourceIndex = static_cast<std::size_t>(row - top) * static_cast<std::size_t>(regionColumns) + static_cast<std::size_t>(column - left);
            const auto targetIndex = index(column, row);
            m_cells[targetIndex] = std::move(region[sourceIndex]);
            mark_dirty(targetIndex);
        }
    }
}

std::size_t RenderModel::index(int column, int row) const
{
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(m_columns) + static_cast<std::size_t>(column);
}

void RenderModel::mark_dirty(std::size_t cellIndex)
{
    if (cellIndex < m_cells.size())
    {
        m_cells[cellIndex].dirty = true;
    }
}

RenderCell RenderModel::blank_cell()
{
    return RenderCell{};
}

} // namespace vklive_nvim
