/* DXGK_PRESENTALLOCATIONINFO: an allocation DxgkDdiPresent reads for a context
 * with GPU virtual addressing. A DDI record; src/km/ddi_record_layout.cpp proves
 * each offset. */
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

}  // namespace av

#endif  // AV_DXGK_PRESENT_ALLOCATION_INFO_RECORD_HPP
