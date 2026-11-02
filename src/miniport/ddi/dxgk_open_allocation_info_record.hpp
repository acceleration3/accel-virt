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

}

#endif
