/* DXGKARG_BUILDPAGINGBUFFER's operation union for
 * DXGK_OPERATION_UNMAP_APERTURE_SEGMENT: the segment pages an allocation is
 * leaving, and the page the aperture should read instead. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGK_BUILD_PAGING_BUFFER_UNMAP_APERTURE_SEGMENT_RECORD_HPP
