/* DXGKARG_DESCRIBEALLOCATION, as DxgkDdiDescribeAllocation receives and
 * answers it. A DDI record: MultisampleMethod and RefreshRate are their two
 * UINTs. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_DESCRIBE_ALLOCATION_RECORD_HPP
#define AV_DXGKARG_DESCRIBE_ALLOCATION_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargDescribeAllocationRecord {
    void     *hAllocation;
    uint32_t Width;
    uint32_t Height;
    int32_t  Format;
    uint32_t MultisampleMethodType;
    uint32_t MultisampleMethodQuality;
    uint32_t RefreshRateNumerator;
    uint32_t RefreshRateDenominator;
    uint32_t PrivateDriverFormatAttribute;
    uint32_t Flags;
    int32_t  Rotation;
};

}  // namespace av

#endif  // AV_DXGKARG_DESCRIBE_ALLOCATION_RECORD_HPP
