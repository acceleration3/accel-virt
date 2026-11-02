/* The rows of a guest surface's pages that differ from what was last sent
 * of it: DWM's window surfaces are redrawn by the guest and only the changed
 * band should cross to the host. The scan owns the shadow it compares against
 * and keeps it equal to what it has reported. The pages are read with
 * streaming loads: they are write-combined, and an ordinary load from them is
 * an uncached trip to memory per sixteen bytes, about 80 MB/s. */
#ifndef AV_D3D_GUEST_SURFACE_SCAN_HPP
#define AV_D3D_GUEST_SURFACE_SCAN_HPP

#include "icd/d3d/row_band.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class GuestSurfaceScan {
public:
    GuestSurfaceScan(uint8_t *shadow, size_t pitch, uint32_t height);
    GuestSurfaceScan(const GuestSurfaceScan &) = delete;
    GuestSurfaceScan &operator=(const GuestSurfaceScan &) = delete;

    /* The rows of `pages` that differ from the shadow, first through last
     * differing row; the shadow takes exactly that band. Empty when none. */
    RowBand differing(const uint8_t *pages);
    /* Every row of `pages` into the shadow. */
    void    copy_all(const uint8_t *pages);

private:
    /* Whether row `row` of `pages` differs from the shadow's, or only in its
     * pitch's padding. */
    bool row_differs(const uint8_t *pages, uint32_t row) const;
    /* Rows [first, end) of `pages` into the shadow. */
    void copy_rows(const uint8_t *pages, uint32_t first, uint32_t end);

    uint8_t *shadow_;
    size_t   pitch_;
    uint32_t height_;
};

}  // namespace av::d3d

#endif  // AV_D3D_GUEST_SURFACE_SCAN_HPP
