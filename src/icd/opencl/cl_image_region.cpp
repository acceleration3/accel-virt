#include "icd/opencl/cl_image_region.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClImageRegion::ClImageRegion(const size_t *origin, const size_t *region, size_t row_pitch,
                             size_t slice_pitch, size_t element_bytes)
    : origin_{origin[0], origin[1], origin[2]}, region_{region[0], region[1], region[2]},
      row_pitch_(row_pitch), slice_pitch_(slice_pitch), element_bytes_(element_bytes)
{
    AV_SCOPE("ClImageRegion::ClImageRegion");
}

bool ClImageRegion::valid() const
{
    AV_SCOPE("ClImageRegion::valid");
    if (element_bytes_ == 0u || region_[0] == 0u || region_[1] == 0u || region_[2] == 0u)
        return false;
    size_t packed_row = 0u;
    if (__builtin_mul_overflow(region_[0], element_bytes_, &packed_row)) return false;
    if (row_pitch_ != 0u && row_pitch_ < packed_row) return false;
    size_t packed_slice = 0u;
    if (__builtin_mul_overflow(row_pitch(), region_[1], &packed_slice)) return false;
    if (slice_pitch_ != 0u && slice_pitch_ < packed_slice) return false;
    /* The furthest byte the application named has to be a size_t too, or a
     * later offset would wrap. */
    size_t reach = 0u;
    return !__builtin_mul_overflow(slice_pitch(), region_[2], &reach);
}

size_t ClImageRegion::width() const
{
    AV_SCOPE("ClImageRegion::width");
    return region_[0];
}

size_t ClImageRegion::rows() const
{
    AV_SCOPE("ClImageRegion::rows");
    return region_[1];
}

size_t ClImageRegion::slices() const
{
    AV_SCOPE("ClImageRegion::slices");
    return region_[2];
}

size_t ClImageRegion::element_bytes() const
{
    AV_SCOPE("ClImageRegion::element_bytes");
    return element_bytes_;
}

size_t ClImageRegion::row_bytes() const
{
    AV_SCOPE("ClImageRegion::row_bytes");
    return region_[0] * element_bytes_;
}

size_t ClImageRegion::row_pitch() const
{
    AV_SCOPE("ClImageRegion::row_pitch");
    return row_pitch_ != 0u ? row_pitch_ : row_bytes();
}

size_t ClImageRegion::slice_pitch() const
{
    AV_SCOPE("ClImageRegion::slice_pitch");
    return slice_pitch_ != 0u ? slice_pitch_ : row_pitch() * region_[1];
}

bool ClImageRegion::rows_packed() const
{
    AV_SCOPE("ClImageRegion::rows_packed");
    return row_pitch() == row_bytes();
}

size_t ClImageRegion::row_offset(size_t row, size_t slice) const
{
    AV_SCOPE("ClImageRegion::row_offset");
    return slice * slice_pitch() + row * row_pitch();
}

void ClImageRegion::origin_of(size_t x, size_t row, size_t slice, size_t *into) const
{
    AV_SCOPE("ClImageRegion::origin_of");
    into[0] = origin_[0] + x;
    into[1] = origin_[1] + row;
    into[2] = origin_[2] + slice;
}

}  // namespace av::clicd
