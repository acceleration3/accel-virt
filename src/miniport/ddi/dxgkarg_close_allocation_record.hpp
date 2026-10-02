/* DXGKARG_CLOSEALLOCATION, as DxgkDdiCloseAllocation receives it. A DDI
 * record. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_CLOSE_ALLOCATION_RECORD_HPP
#define AV_DXGKARG_CLOSE_ALLOCATION_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCloseAllocationRecord {
    uint32_t    NumAllocations;
    uint32_t    NumAllocationsPadding;
    void *const *pOpenHandleList;
};

}  // namespace av

#endif  // AV_DXGKARG_CLOSE_ALLOCATION_RECORD_HPP
