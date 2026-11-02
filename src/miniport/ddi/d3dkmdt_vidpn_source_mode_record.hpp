#ifndef AV_D3DKMDT_VIDPN_SOURCE_MODE_RECORD_HPP
#define AV_D3DKMDT_VIDPN_SOURCE_MODE_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtVidPnSourceModeRecord {
    uint32_t Id;
    int32_t  Type;
    uint32_t PrimSurfSizeCx;
    uint32_t PrimSurfSizeCy;
    uint32_t VisibleRegionSizeCx;
    uint32_t VisibleRegionSizeCy;
    uint32_t Stride;
    int32_t  PixelFormat;
    int32_t  ColorBasis;
    int32_t  PixelValueAccessMode;
};

}

#endif
