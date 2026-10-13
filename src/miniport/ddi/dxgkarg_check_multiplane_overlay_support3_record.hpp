/* DXGKARG_CHECKMULTIPLANEOVERLAYSUPPORT3, as
 * DxgkDdiCheckMultiPlaneOverlaySupport3 receives it, with the plane and
 * post-composition records its pointers lead to. A DDI record: ReturnInfo is
 * the DXGK_CHECK_MULTIPLANE_OVERLAY_SUPPORT_RETURN_INFO Value;
 * src/km/ddi_record_layout.cpp proves each offset and the sizes. */
#ifndef AV_DXGKARG_CHECK_MULTIPLANE_OVERLAY_SUPPORT3_RECORD_HPP
#define AV_DXGKARG_CHECK_MULTIPLANE_OVERLAY_SUPPORT3_RECORD_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"

#include <stdint.h>

namespace av {

/* DXGK_MULTIPLANE_OVERLAY_PLANE_WITH_SOURCE2. */
struct DxgkMultiplaneOverlayPlaneWithSource2Record {
    void                                  *hAllocation;
    uint32_t                               VidPnSourceId;
    uint32_t                               LayerIndex;
    DxgkMultiplaneOverlayAttributes3Record PlaneAttributes;
};

/* DXGK_MULTIPLANE_OVERLAY_POST_COMPOSITION_WITH_SOURCE. */
struct DxgkMultiplaneOverlayPostCompositionWithSourceRecord {
    uint32_t       VidPnSourceId;
    uint32_t       Flags;      /* DXGK_MULTIPLANE_OVERLAY_POST_COMPOSITION_FLAGS */
    RectRecord SrcRect;
    RectRecord DstRect;
    uint32_t       Rotation;   /* D3DDDI_ROTATION */
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

/* DXGK_CHECK_MULTIPLANE_OVERLAY_SUPPORT_RETURN_INFO.FailingPlane, four bits. */
constexpr uint32_t check_failing_plane_mask = 0xFu;

}  // namespace av

#endif  // AV_DXGKARG_CHECK_MULTIPLANE_OVERLAY_SUPPORT3_RECORD_HPP
