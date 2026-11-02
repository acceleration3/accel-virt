#ifndef AV_DXGK_PRESENT_ALLOCATION_INFO_RECORD_HPP
#define AV_DXGK_PRESENT_ALLOCATION_INFO_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkPresentAllocationInfoRecord {
    void     *hDeviceSpecificAllocation;
    uint64_t AllocationVirtualAddress;
    int64_t  PhysicalAddress;
    uint16_t SegmentId;
    uint16_t PhysicalAdapterIndex;
    uint32_t Padding;
};

}

#endif
