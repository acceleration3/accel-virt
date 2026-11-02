#ifndef AV_D3DDDI_PATCH_LOCATION_LIST_RECORD_HPP
#define AV_D3DDDI_PATCH_LOCATION_LIST_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dddiPatchLocationListRecord {
    uint32_t AllocationIndex;
    uint32_t SlotId;
    uint32_t DriverId;
    uint32_t AllocationOffset;
    uint32_t PatchOffset;
    uint32_t SplitOffset;
};

}

#endif
