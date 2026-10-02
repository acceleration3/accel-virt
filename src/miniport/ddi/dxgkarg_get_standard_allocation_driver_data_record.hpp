/* DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, as
 * DxgkDdiGetStandardAllocationDriverData receives and answers it. A DDI
 * record: pCreateSurfaceData is the union of the per-type surface data
 * pointers. src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGKARG_GET_STANDARD_ALLOCATION_DRIVER_DATA_RECORD_HPP
