#ifndef AV_D3DKMDT_SHADOW_SURFACE_DATA_RECORD_HPP
#define AV_D3DKMDT_SHADOW_SURFACE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtShadowSurfaceDataRecord {
    uint32_t Width;
    uint32_t Height;
    int32_t  Format;
    uint32_t Pitch;
};

}

#endif
