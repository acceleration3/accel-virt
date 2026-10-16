/* One surface's presents so far, each of which names a point on it: the
 * frame the host staged there, which the miniport moves to the window when
 * the kernel's present names the same point (docs/shared-surfaces.md). Point
 * 0 names no frame, so the count never answers it: past 2^32 - 1 presents it
 * wraps to 1. Direct3D 9, 11 and 12 each keep one per presented surface. */
#ifndef AV_D3D_PRESENT_COUNT_HPP
#define AV_D3D_PRESENT_COUNT_HPP

#include <stdint.h>

namespace av::d3d {

class PresentCount {
public:
    PresentCount();

    /* Counts one present more and answers the point it publishes. */
    uint32_t next();
    /* The point the last present published; 0 before any. */
    uint32_t last() const;

private:
    uint32_t count_;
};

}  // namespace av::d3d

#endif  // AV_D3D_PRESENT_COUNT_HPP
