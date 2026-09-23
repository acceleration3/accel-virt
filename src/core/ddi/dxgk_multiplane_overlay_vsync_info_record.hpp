/* DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO, one layer of a
 * CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY interrupt: 0x68 bytes, the record
 * VidSchiProcessIsrVSync (dxgmms2.sys @140030b04) reads. A DDI record:
 * PlaneAttributes is DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES, handed back as the
 * flip gave it, since dxgmms2 compares each enabled layer's rectangles with
 * the flip's; src/km/ddi_record_layout.cpp proves each offset and the size. */
#ifndef AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO_RECORD_HPP

#include "core/ddi/dxgk_multiplane_overlay_plane_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayVsyncInfoRecord {
    uint32_t LayerIndex;
    uint32_t Enabled;
    int64_t  PhysicalAddress;
    uint32_t PlaneAttributes[multiplane_overlay_attribute_words];
};

}  // namespace av

#endif  // AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO_RECORD_HPP
