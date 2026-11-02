#ifndef AV_DXGK_PRIMARY_DATA_RECORD_HPP
#define AV_DXGK_PRIMARY_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkPrimaryDataRecord {
    void    *hAllocation;
    uint16_t SegmentId;
    uint8_t  SegmentIdPadding[6];
    int64_t  SegmentAddress;
};

}

#endif
