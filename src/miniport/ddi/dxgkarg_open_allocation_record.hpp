/* DXGKARG_OPENALLOCATION, as DxgkDdiOpenAllocation receives and answers it. A
 * DDI record: Flags is the DXGK_OPENALLOCATIONFLAGS Value.
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_OPEN_ALLOCATION_RECORD_HPP
#define AV_DXGKARG_OPEN_ALLOCATION_RECORD_HPP

#include "miniport/ddi/dxgk_open_allocation_info_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargOpenAllocationRecord {
    uint32_t                     NumAllocations;
    uint32_t                     NumAllocationsPadding;
    DxgkOpenAllocationInfoRecord *pOpenAllocation;
    void                         *pPrivateDriverData;
    uint32_t                     PrivateDriverSize;
    uint32_t                     Flags;
    uint32_t                     SubresourceIndex;
    uint32_t                     SubresourceIndexPadding;
    uint64_t                     SubresourceOffset;
    uint32_t                     Pitch;
    uint32_t                     PitchPadding;
};

}  // namespace av

#endif  // AV_DXGKARG_OPEN_ALLOCATION_RECORD_HPP
