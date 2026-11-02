#ifndef AV_DXGKARG_DESTROY_ALLOCATION_RECORD_HPP
#define AV_DXGKARG_DESTROY_ALLOCATION_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargDestroyAllocationRecord {
    uint32_t    NumAllocations;
    uint32_t    NumAllocationsPadding;
    void *const *pAllocationList;
    void        *hResource;
    uint32_t    Flags;
    uint32_t    FlagsPadding;
};

}

#endif
