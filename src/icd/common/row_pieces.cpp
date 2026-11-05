#include "icd/common/row_pieces.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

RowPieces::RowPieces(const TexelLayout &layout, uint32_t width, uint32_t height, uint32_t pitch, uint64_t most)
    : RowPieces(layout, width, height, pitch, most, SplitReport())
{
    AV_SCOPE("RowPieces::RowPieces");
}

RowPieces::RowPieces(const TexelLayout &layout, uint32_t width, uint32_t height, uint32_t pitch, uint64_t most,
                     ClientTransport &wire)
    : RowPieces(layout, width, height, pitch, most, SplitReport(wire))
{
    AV_SCOPE("RowPieces::RowPieces");
}

RowPieces::RowPieces(const TexelLayout &layout, uint32_t width, uint32_t height, uint32_t pitch, uint64_t most,
                     const SplitReport &report)
    : layout_(layout), width_(width), height_(height), pitch_(pitch), rows_(0u), columns_(0u), rows_per_piece_(0u),
      columns_per_piece_(0u), row_(0u), column_(0u), row_count_(0u), column_count_(0u), report_(report)
{
    AV_SCOPE("RowPieces::RowPieces");
    const uint64_t element = layout.element_bytes();
    if (element == 0u || most < element) return;
    rows_ = layout.rows(height);
    columns_ = layout.row_bytes(width) / layout.element_bytes();
    const uint64_t line = uint64_t{columns_} * element;
    if (line > most) {
        rows_per_piece_ = 1u;
        columns_per_piece_ = static_cast<uint32_t>(most / element);
        return;
    }
    columns_per_piece_ = columns_;
    const uint64_t fit = pitch == 0u ? rows_ : 1u + (most - line) / pitch;
    rows_per_piece_ = fit < rows_ ? static_cast<uint32_t>(fit) : rows_;
}

bool RowPieces::next()
{
    AV_SCOPE("RowPieces::next");
    if (columns_ == 0u) return false;
    column_ += column_count_;
    if (column_ >= columns_) {
        column_ = 0u;
        row_ += row_count_;
    }
    if (row_ >= rows_) return false;
    row_count_ = rows_ - row_ < rows_per_piece_ ? rows_ - row_ : rows_per_piece_;
    column_count_ = columns_ - column_ < columns_per_piece_ ? columns_ - column_ : columns_per_piece_;
    report_.piece();
    return true;
}

uint32_t RowPieces::top() const
{
    AV_SCOPE("RowPieces::top");
    return row_ * layout_.element_height();
}

uint32_t RowPieces::bottom() const
{
    AV_SCOPE("RowPieces::bottom");
    const uint32_t end = (row_ + row_count_) * layout_.element_height();
    return end < height_ ? end : height_;
}

uint32_t RowPieces::left() const
{
    AV_SCOPE("RowPieces::left");
    return column_ * layout_.element_width();
}

uint32_t RowPieces::right() const
{
    AV_SCOPE("RowPieces::right");
    const uint32_t end = (column_ + column_count_) * layout_.element_width();
    return end < width_ ? end : width_;
}

uint64_t RowPieces::offset() const
{
    AV_SCOPE("RowPieces::offset");
    return uint64_t{row_} * pitch_ + uint64_t{column_} * layout_.element_bytes();
}

uint64_t RowPieces::bytes() const
{
    AV_SCOPE("RowPieces::bytes");
    if (row_count_ == 0u) return 0u;
    return uint64_t{row_count_ - 1u} * pitch_ + uint64_t{column_count_} * layout_.element_bytes();
}

bool RowPieces::first() const
{
    AV_SCOPE("RowPieces::first");
    return row_ == 0u && column_ == 0u;
}

}
