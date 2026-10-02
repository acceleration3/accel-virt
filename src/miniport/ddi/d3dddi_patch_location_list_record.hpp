/* D3DDDI_PATCHLOCATIONLIST, one place in a DMA buffer an allocation address
 * is written. A DDI record: SlotId is the union Value.
 * src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_D3DDDI_PATCH_LOCATION_LIST_RECORD_HPP
