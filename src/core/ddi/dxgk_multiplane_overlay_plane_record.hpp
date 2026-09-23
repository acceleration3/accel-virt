/* DXGK_MULTIPLANE_OVERLAY_PLANE, one plane of a multiplane flip. A DDI
 * record: PlaneAttributes is DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES, kept whole
 * because only the base plane's address is read; src/km/ddi_record_layout.cpp
 * proves each offset and the size, which is the array's stride. */
#ifndef AV_DXGK_MULTIPLANE_OVERLAY_PLANE_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_PLANE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayPlaneRecord {
    uint32_t LayerIndex;
    uint32_t Enabled;
    uint32_t AllocationSegment;
    uint32_t AllocationSegmentPadding;
    int64_t  AllocationAddress;
    void    *hAllocation;
    uint32_t PlaneAttributes[22];
};

}  // namespace av

#endif  // AV_DXGK_MULTIPLANE_OVERLAY_PLANE_RECORD_HPP
