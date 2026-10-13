/* DXGKARG_SETVIDPNSOURCEADDRESSWITHMULTIPLANEOVERLAY3, as
 * DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay3 receives it, with the
 * plane and per-context records its pointers lead to. A DDI record: the flags
 * are their Values; src/km/ddi_record_layout.cpp proves each offset and the
 * plane's size. */
#ifndef AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_WITH_MULTI_PLANE_OVERLAY3_RECORD_HPP
#define AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_WITH_MULTI_PLANE_OVERLAY3_RECORD_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"

#include <stdint.h>

namespace av {

/* DXGK_PRIMARYCONTEXTDATA: one context's view of a plane's allocation. */
struct DxgkPrimaryContextDataRecord {
    void    *hContext;
    void    *hAllocation;
    uint16_t SegmentId;
    uint16_t SegmentIdPadding[3];
    int64_t  SegmentAddress;
};

/* DXGK_MULTIPLANE_OVERLAY_PLANE3. */
struct DxgkMultiplaneOverlayPlane3Record {
    uint32_t                       LayerIndex;
    uint32_t                       LayerIndexPadding;
    uint64_t                       PresentId;
    uint32_t                       InputFlags;    /* DXGK_PLANE_SPECIFIC_INPUT_FLAGS */
    uint32_t                       OutputFlags;   /* DXGK_PLANE_SPECIFIC_OUTPUT_FLAGS */
    uint32_t                       MaxImmediateFlipLine;
    uint32_t                       ContextCount;
    DxgkPrimaryContextDataRecord **ppContextData;
    uint32_t                       DriverPrivateDataSize;
    uint32_t                       DriverPrivateDataSizePadding;
    void                          *pDriverPrivateData;
    DxgkMultiplaneOverlayAttributes3Record PlaneAttributes;
};

/* DXGK_PLANE_SPECIFIC_INPUT_FLAGS.Enabled and FlipImmediate. */
constexpr uint32_t plane_input_enabled = 0x1u;
constexpr uint32_t plane_input_flip_immediate = 0x2u;

struct DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record {
    uint32_t                            VidPnSourceId;
    uint32_t                            InputFlags;    /* DXGK_SETVIDPNSOURCEADDRESS_INPUT_FLAGS */
    uint32_t                            OutputFlags;   /* DXGK_SETVIDPNSOURCEADDRESS_OUTPUT_FLAGS */
    uint32_t                            PlaneCount;
    DxgkMultiplaneOverlayPlane3Record **ppPlanes;
    void                               *pPostComposition;
    uint32_t                            Duration;
    uint32_t                            DurationPadding;
    void                               *pHDRMetaData;
    uint64_t                            TargetFlipTime;
};

}  // namespace av

#endif  // AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_WITH_MULTI_PLANE_OVERLAY3_RECORD_HPP
