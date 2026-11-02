#ifndef AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO_RECORD_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayVsyncInfoRecord {
    uint32_t LayerIndex;
    uint32_t Enabled;
    int64_t  PhysicalAddress;
    uint32_t PlaneAttributes[multiplane_overlay_attribute_words];
};

}

#endif
