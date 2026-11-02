#ifndef AV_DXGKARG_CREATE_ALLOCATION_RECORD_HPP
#define AV_DXGKARG_CREATE_ALLOCATION_RECORD_HPP

#include "miniport/ddi/dxgk_allocation_info_record.hpp"

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

}

#endif
