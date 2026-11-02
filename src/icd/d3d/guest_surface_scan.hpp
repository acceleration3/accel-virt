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

    RowBand differing(const uint8_t *pages);

private:
    uint8_t *shadow_;
    size_t   pitch_;
    uint32_t height_;
};

}

#endif
