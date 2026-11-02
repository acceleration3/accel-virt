#ifndef AV_DXGKARG_CLOSE_ALLOCATION_RECORD_HPP
#define AV_DXGKARG_CLOSE_ALLOCATION_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCloseAllocationRecord {
    uint32_t    NumAllocations;
    uint32_t    NumAllocationsPadding;
    void *const *pOpenHandleList;
};

}

#endif
