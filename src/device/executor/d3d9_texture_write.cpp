#include "device/executor/d3d9_texture_write.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"

namespace av {

D3d9TextureWrite::D3d9TextureWrite(const d3d9proto::Box &box, uint32_t row_pitch, uint32_t slice_pitch,
                                   uint64_t data_size, const D3d9FormatLayout &layout)
    : box_(box), row_pitch_(row_pitch), slice_pitch_(slice_pitch), data_size_(data_size), layout_(layout)
{
    AV_SCOPE("D3d9TextureWrite::D3d9TextureWrite");
}

bool D3d9TextureWrite::spans(uint32_t first, uint32_t last, uint32_t extent, uint32_t element) const
{
    AV_SCOPE("D3d9TextureWrite::spans");
    return first < last && last <= extent && first % element == 0u && (last % element == 0u || last == extent);
}

bool D3d9TextureWrite::fits(uint32_t width, uint32_t height, uint32_t depth) const
{
    AV_SCOPE("D3d9TextureWrite::fits");
    if (layout_.element_bytes() == 0u || !spans(box_.left, box_.right, width, layout_.element_width()) ||
        !spans(box_.top, box_.bottom, height, layout_.element_height()) || !spans(box_.front, box_.back, depth, 1u))
        return false;
    if (row_pitch_ < row_bytes() || (slices() > 1u && uint64_t{slice_pitch_} < uint64_t{row_pitch_} * rows()))
        return false;
    const uint64_t last_slice = uint64_t{slices() - 1u} * slice_pitch_;
    const uint64_t last_row = uint64_t{rows() - 1u} * row_pitch_;
    return last_slice + last_row + row_bytes() <= data_size_;
}

uint32_t D3d9TextureWrite::rows() const
{
    AV_SCOPE("D3d9TextureWrite::rows");
    return layout_.rows(box_.bottom - box_.top);
}

uint32_t D3d9TextureWrite::row_bytes() const
{
    AV_SCOPE("D3d9TextureWrite::row_bytes");
    return layout_.row_bytes(box_.right - box_.left);
}

uint32_t D3d9TextureWrite::slices() const
{
    AV_SCOPE("D3d9TextureWrite::slices");
    return box_.back - box_.front;
}

}  // namespace av
