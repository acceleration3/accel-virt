/* DXGK_ALLOCATIONLIST, one allocation a submission references. A DDI record:
 * SegmentIdBits is the anonymous WriteOperation:1, SegmentId:5 bitfield.
 * src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGK_ALLOCATION_LIST_RECORD_HPP
