#ifndef AV_D3DKMDT_SHARED_PRIMARY_SURFACE_DATA_RECORD_HPP
#define AV_D3DKMDT_SHARED_PRIMARY_SURFACE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtSharedPrimarySurfaceDataRecord {
    uint32_t Width;
    uint32_t Height;
    int32_t  Format;
    uint32_t RefreshRateNumerator;
    uint32_t RefreshRateDenominator;
    uint32_t VidPnSourceId;
};

}

#endif
