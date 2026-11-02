#ifndef AV_DXGKARG_CHECK_MULTIPLANE_OVERLAY_SUPPORT3_RECORD_HPP
#define AV_DXGKARG_CHECK_MULTIPLANE_OVERLAY_SUPPORT3_RECORD_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"

#include <stdint.h>

namespace av {
struct DxgkMultiplaneOverlayPlaneWithSource2Record {
    void                                  *hAllocation;
    uint32_t                               VidPnSourceId;
    uint32_t                               LayerIndex;
    DxgkMultiplaneOverlayAttributes3Record PlaneAttributes;
};

struct DxgkMultiplaneOverlayPostCompositionWithSourceRecord {
    uint32_t       VidPnSourceId;
    uint32_t       Flags;
    RectRecord SrcRect;
    RectRecord DstRect;
    uint32_t       Rotation;
};

struct DxgkargCheckMultiPlaneOverlaySupport3Record {
    uint32_t                                               PlaneCount;
    uint32_t                                               PlaneCountPadding;
    DxgkMultiplaneOverlayPlaneWithSource2Record          **ppPlanes;
    uint32_t                                               PostCompositionCount;
    uint32_t                                               PostCompositionCountPadding;
    DxgkMultiplaneOverlayPostCompositionWithSourceRecord **ppPostComposition;
    int32_t                                                Supported;
    uint32_t                                               ReturnInfo;
};

constexpr uint32_t check_failing_plane_mask = 0xFu;

}

#endif
