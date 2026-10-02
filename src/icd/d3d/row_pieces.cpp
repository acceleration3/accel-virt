#include "icd/d3d/row_pieces.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

RowPieces::RowPieces(uint32_t rows, uint32_t texel_rows, uint32_t line, uint32_t pitch, uint64_t most)
    : rows_(rows), texel_rows_(texel_rows), line_(line), pitch_(pitch),
      per_piece_(pitch == 0u || pitch >= most ? 1u : static_cast<uint32_t>(most / pitch)),
      texels_per_row_(rows == 0u ? 1u : (texel_rows + rows - 1u) / rows), done_(0u), count_(0u)
{
    AV_SCOPE("RowPieces::RowPieces");
}

bool RowPieces::next()
{
    AV_SCOPE("RowPieces::next");
    done_ += count_;
    if (done_ >= rows_) return false;
    count_ = rows_ - done_ < per_piece_ ? rows_ - done_ : per_piece_;
    return true;
}

uint32_t RowPieces::top() const
{
    AV_SCOPE("RowPieces::top");
    return done_ * texels_per_row_;
}

uint32_t RowPieces::bottom() const
{
    AV_SCOPE("RowPieces::bottom");
    const uint32_t end = (done_ + count_) * texels_per_row_;
    return end < texel_rows_ ? end : texel_rows_;
}

uint64_t RowPieces::offset() const
{
    AV_SCOPE("RowPieces::offset");
    return uint64_t{done_} * pitch_;
}

bool RowPieces::first() const
{
    AV_SCOPE("RowPieces::first");
    return done_ == 0u;
}

uint64_t RowPieces::bytes() const
{
    AV_SCOPE("RowPieces::bytes");
    return count_ == 0u ? 0u : uint64_t{count_ - 1u} * pitch_ + line_;
}

}  // namespace av::d3d
