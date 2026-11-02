#ifndef AV_D3DKMDT_STAGING_SURFACE_DATA_RECORD_HPP
#define AV_D3DKMDT_STAGING_SURFACE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtStagingSurfaceDataRecord {
    uint32_t Width;
    uint32_t Height;
    uint32_t Pitch;
};

}

#endif
