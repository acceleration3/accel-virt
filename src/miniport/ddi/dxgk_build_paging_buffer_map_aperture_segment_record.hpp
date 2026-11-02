#ifndef AV_DXGK_BUILD_PAGING_BUFFER_MAP_APERTURE_SEGMENT_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_MAP_APERTURE_SEGMENT_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferMapApertureSegmentRecord {
    void    *hDevice;
    void    *hAllocation;
    uint32_t SegmentId;
    uint32_t SegmentIdPadding;
    uint64_t OffsetInPages;
    uint64_t NumberOfPages;
    void    *pMdl;
    uint32_t Flags;
    uint32_t MdlOffset;
};

}

#endif
