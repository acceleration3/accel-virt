/* DXGK_OPENALLOCATIONINFO, one allocation of a DxgkDdiOpenAllocation call. A
 * DDI record. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGK_OPEN_ALLOCATION_INFO_RECORD_HPP
#define AV_DXGK_OPEN_ALLOCATION_INFO_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkOpenAllocationInfoRecord {
    uint32_t hAllocation;
    uint32_t hAllocationPadding;
    void     *pPrivateDriverData;
    uint32_t PrivateDriverDataSize;
    uint32_t PrivateDriverDataSizePadding;
    void     *hDeviceSpecificAllocation;
};

}  // namespace av

#endif  // AV_DXGK_OPEN_ALLOCATION_INFO_RECORD_HPP
