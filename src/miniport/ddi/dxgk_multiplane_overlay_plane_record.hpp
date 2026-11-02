#ifndef AV_DXGK_MULTIPLANE_OVERLAY_PLANE_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_PLANE_RECORD_HPP

#include <stdint.h>

namespace av {
constexpr uint32_t multiplane_overlay_attribute_words = 22u;

struct DxgkMultiplaneOverlayPlaneRecord {
    uint32_t LayerIndex;
    uint32_t Enabled;
    uint32_t AllocationSegment;
    uint32_t AllocationSegmentPadding;
    int64_t  AllocationAddress;
    void    *hAllocation;
    uint32_t PlaneAttributes[multiplane_overlay_attribute_words];
};

}

#endif
