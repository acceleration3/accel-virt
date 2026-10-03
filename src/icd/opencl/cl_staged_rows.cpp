#include "icd/opencl/cl_staged_rows.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/row_pieces.hpp"
#include "icd/opencl/cl_image_region.hpp"

namespace av::clicd {

ClStagedRows::ClStagedRows(const ClImageRegion &layout, const icd::RowPieces &piece, size_t slice)
    : layout_(layout), slice_(slice), top_(piece.top()), rows_(piece.bottom() - piece.top()),
      column_(piece.left()), row_bytes_((piece.right() - piece.left()) * layout.element_bytes())
{
    AV_SCOPE("ClStagedRows::ClStagedRows");
}

void ClStagedRows::pack(const uint8_t *application, uint8_t *staged) const
{
    AV_SCOPE("ClStagedRows::pack");
    for (size_t index = 0u; index < rows_; ++index)
        __builtin_memcpy(staged + index * row_bytes_, application + row(index), row_bytes_);
}

void ClStagedRows::unpack(const uint8_t *staged, uint8_t *application) const
{
    AV_SCOPE("ClStagedRows::unpack");
    for (size_t index = 0u; index < rows_; ++index)
        __builtin_memcpy(application + row(index), staged + index * row_bytes_, row_bytes_);
}

size_t ClStagedRows::row(size_t index) const
{
    AV_SCOPE("ClStagedRows::row");
    return layout_.row_offset(top_ + index, slice_) + column_ * layout_.element_bytes();
}

}  // namespace av::clicd
