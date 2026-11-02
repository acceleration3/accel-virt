#ifndef AV_D3DKMDT_GDI_SURFACE_DATA_RECORD_HPP
#define AV_D3DKMDT_GDI_SURFACE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtGdiSurfaceDataRecord {
    uint32_t Width;
    uint32_t Height;
    int32_t  Format;
    int32_t  Type;
    uint32_t Flags;
    uint32_t Pitch;
};

}

#endif
