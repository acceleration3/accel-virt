/* DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO2, one layer of a
 * CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY2 interrupt: the layer and the PresentId
 * its flip gave it, which is all VidSchiIsExpectedVSyncCookie (dxgmms2.sys)
 * compares for a flip made through the MPO3 DDI. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset and the size. */
#ifndef AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO2_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO2_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayVsyncInfo2Record {
    uint32_t LayerIndex;
    uint32_t LayerIndexPadding;
    uint64_t PresentId;
    uint32_t Flags;   /* DXGKCB_NOTIFY_MPO_VSYNC_FLAGS */
    uint32_t FlagsPadding;
};

}  // namespace av

#endif  // AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO2_RECORD_HPP
