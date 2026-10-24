#include "icd/d3d/guest_surface_scan.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

GuestSurfaceScan::GuestSurfaceScan(uint8_t *shadow, size_t pitch, uint32_t height)
    : shadow_(shadow), pitch_(pitch), height_(height)
{
    AV_SCOPE("GuestSurfaceScan::GuestSurfaceScan");
}

RowBand GuestSurfaceScan::differing(const uint8_t *pages)
{
    AV_SCOPE("GuestSurfaceScan::differing");
    uint32_t first = height_, last = 0u;
    for (uint32_t row = 0u; row < height_; row++) {
        if (__builtin_memcmp(pages + row * pitch_, shadow_ + row * pitch_, pitch_) == 0) continue;
        if (first == height_) first = row;
        last = row;
    }
    if (first == height_) return RowBand();
    const size_t offset = first * pitch_;
    __builtin_memcpy(shadow_ + offset, pages + offset, (last + 1u - first) * pitch_);
    return RowBand(first, last + 1u);
}

}  // namespace av::d3d
