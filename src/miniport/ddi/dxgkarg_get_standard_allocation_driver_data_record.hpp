#ifndef AV_DXGKARG_GET_STANDARD_ALLOCATION_DRIVER_DATA_RECORD_HPP
#define AV_DXGKARG_GET_STANDARD_ALLOCATION_DRIVER_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargGetStandardAllocationDriverDataRecord {
    int32_t  StandardAllocationType;
    uint32_t StandardAllocationTypePadding;
    void     *pCreateSurfaceData;
    void     *pAllocationPrivateDriverData;
    uint32_t AllocationPrivateDriverDataSize;
    uint32_t AllocationPrivateDriverDataSizePadding;
    void     *pResourcePrivateDriverData;
    uint32_t ResourcePrivateDriverDataSize;
    uint32_t PhysicalAdapterIndex;
};

}

#endif
