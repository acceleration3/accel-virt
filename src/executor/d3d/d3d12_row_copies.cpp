#include "executor/d3d/d3d12_row_copies.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d12RowCopies::D3d12RowCopies(uint32_t format, uint32_t footprint_height, uint32_t row_pitch)
    : layout_(format), height_(footprint_height), row_pitch_(row_pitch)
{
    AV_SCOPE("D3d12RowCopies::D3d12RowCopies");
}

bool D3d12RowCopies::needed() const
{
    AV_SCOPE("D3d12RowCopies::needed");
    return layout_.known() && layout_.element_width() == 1u && layout_.element_height() == 1u &&
           layout_.element_bytes() != 0u && row_pitch_ % layout_.element_bytes() != 0u;
}

uint64_t D3d12RowCopies::row_offset(uint64_t base, uint32_t slice, uint32_t row, uint32_t left) const
{
    AV_SCOPE("D3d12RowCopies::row_offset");
    const uint64_t slice_pitch = static_cast<uint64_t>(row_pitch_) * layout_.rows(height_);
    return base + slice * slice_pitch + static_cast<uint64_t>(row) * row_pitch_ +
           static_cast<uint64_t>(left) * layout_.element_bytes();
}

uint32_t D3d12RowCopies::pitch_of(uint32_t width) const
{
    AV_SCOPE("D3d12RowCopies::pitch_of");
    return layout_.row_bytes(width);
}

}
