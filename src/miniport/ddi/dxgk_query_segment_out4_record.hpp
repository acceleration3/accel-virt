#ifndef AV_DXGK_QUERY_SEGMENT_OUT4_RECORD_HPP
#define AV_DXGK_QUERY_SEGMENT_OUT4_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkQuerySegmentOut4Record {
    uint32_t NbSegment;
    uint32_t NbSegmentPadding;
    uint8_t  *pSegmentDescriptor;
    uint32_t PagingBufferSegmentId;
    uint32_t PagingBufferSize;
    uint32_t PagingBufferPrivateDataSize;
    uint32_t PagingBufferPrivateDataSizePadding;
    uint64_t SegmentDescriptorStride;
};

}

#endif
