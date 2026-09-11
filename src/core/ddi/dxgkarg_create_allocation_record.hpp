/* DXGKARG_CREATEALLOCATION, as DxgkDdiCreateAllocation receives and answers
 * it. A DDI record: Flags is the DXGK_CREATEALLOCATIONFLAGS Value.
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_CREATE_ALLOCATION_RECORD_HPP
#define AV_DXGKARG_CREATE_ALLOCATION_RECORD_HPP

#include "dxgk_allocation_info_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargCreateAllocationRecord {
    const void               *pPrivateDriverData;
    uint32_t                 PrivateDriverDataSize;
    uint32_t                 NumAllocations;
    DxgkAllocationInfoRecord *pAllocationInfo;
    void                     *hResource;
    uint32_t                 Flags;
    uint32_t                 FlagsPadding;
};

}  // namespace av

#endif  // AV_DXGKARG_CREATE_ALLOCATION_RECORD_HPP
