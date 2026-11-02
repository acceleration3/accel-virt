#ifndef AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO2_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_VSYNC_INFO2_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayVsyncInfo2Record {
    uint32_t LayerIndex;
    uint32_t LayerIndexPadding;
    uint64_t PresentId;
    uint32_t Flags;
    uint32_t FlagsPadding;
};

}

#endif
