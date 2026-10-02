/* DXGKARG_BUILDPAGINGBUFFER's operation union for
 * DXGK_OPERATION_MAP_APERTURE_SEGMENT: the segment pages an allocation is being
 * mapped into, and the MDL of the guest pages that back them. A DDI record:
 * Flags is the DXGK_MAPAPERTUREFLAGS Value, and MdlOffset is in pages;
 * src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGK_BUILD_PAGING_BUFFER_MAP_APERTURE_SEGMENT_RECORD_HPP
