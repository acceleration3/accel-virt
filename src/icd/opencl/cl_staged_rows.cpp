#include "icd/opencl/cl_staged_rows.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/row_pieces.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "proto/resources/pitched_rows.hpp"

namespace av::clicd {

ClStagedRows::ClStagedRows(const ClImageRegion &layout, const icd::RowPieces &piece, size_t slice)
    : layout_(layout), slice_(slice), top_(piece.top()), rows_(piece.bottom() - piece.top()),
      column_(piece.left()), row_bytes_((piece.right() - piece.left()) * layout.element_bytes())
{
    AV_SCOPE("ClStagedRows::ClStagedRows");
}

uint64_t ClStagedRows::spread() const
{
    AV_SCOPE("ClStagedRows::spread");
    return rows_ == 0u ? 0u : uint64_t{rows_ - 1u} * layout_.row_pitch() + row_bytes_;
}

void ClStagedRows::pack(const uint8_t *application, uint8_t *staged) const
{
    AV_SCOPE("ClStagedRows::pack");
    static_cast<void>(PitchedRows(staged, row_bytes_, uint64_t{rows_} * row_bytes_)
                          .copy_lines(PitchedRows(application + row(0u), layout_.row_pitch(), spread()),
                                      static_cast<uint32_t>(rows_), row_bytes_));
}

void ClStagedRows::unpack(const uint8_t *staged, uint8_t *application) const
{
    AV_SCOPE("ClStagedRows::unpack");
    static_cast<void>(PitchedRows(application + row(0u), layout_.row_pitch(), spread())
                          .copy_lines(PitchedRows(staged, row_bytes_, uint64_t{rows_} * row_bytes_),
                                      static_cast<uint32_t>(rows_), row_bytes_));
}

size_t ClStagedRows::row(size_t index) const
{
    AV_SCOPE("ClStagedRows::row");
    return layout_.row_offset(top_ + index, slice_) + column_ * layout_.element_bytes();
}

}
