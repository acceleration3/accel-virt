#ifndef AV_DXGK_BUILD_PAGING_BUFFER_UNMAP_APERTURE_SEGMENT_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_UNMAP_APERTURE_SEGMENT_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferUnmapApertureSegmentRecord {
    void    *hDevice;
    void    *hAllocation;
    uint32_t SegmentId;
    uint32_t SegmentIdPadding;
    uint64_t OffsetInPages;
    uint64_t NumberOfPages;
    int64_t  DummyPage;
};

}

#endif
