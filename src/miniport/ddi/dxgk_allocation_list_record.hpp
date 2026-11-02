#ifndef AV_DXGK_ALLOCATION_LIST_RECORD_HPP
#define AV_DXGK_ALLOCATION_LIST_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkAllocationListRecord {
    void     *hDeviceSpecificAllocation;
    uint32_t SegmentIdBits;
    uint32_t SegmentIdBitsPadding;
    int64_t  PhysicalAddress;
};

}

#endif
