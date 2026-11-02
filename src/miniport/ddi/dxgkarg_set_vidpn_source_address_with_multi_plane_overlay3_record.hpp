#ifndef AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_WITH_MULTI_PLANE_OVERLAY3_RECORD_HPP
#define AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_WITH_MULTI_PLANE_OVERLAY3_RECORD_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"

#include <stdint.h>

namespace av {
struct DxgkPrimaryContextDataRecord {
    void    *hContext;
    void    *hAllocation;
    uint16_t SegmentId;
    uint16_t SegmentIdPadding[3];
    int64_t  SegmentAddress;
};

struct DxgkMultiplaneOverlayPlane3Record {
    uint32_t                       LayerIndex;
    uint32_t                       LayerIndexPadding;
    uint64_t                       PresentId;
    uint32_t                       InputFlags;
    uint32_t                       OutputFlags;
    uint32_t                       MaxImmediateFlipLine;
    uint32_t                       ContextCount;
    DxgkPrimaryContextDataRecord **ppContextData;
    uint32_t                       DriverPrivateDataSize;
    uint32_t                       DriverPrivateDataSizePadding;
    void                          *pDriverPrivateData;
    DxgkMultiplaneOverlayAttributes3Record PlaneAttributes;
};

constexpr uint32_t plane_input_enabled = 0x1u;
constexpr uint32_t plane_input_flip_immediate = 0x2u;

struct DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record {
    uint32_t                            VidPnSourceId;
    uint32_t                            InputFlags;
    uint32_t                            OutputFlags;
    uint32_t                            PlaneCount;
    DxgkMultiplaneOverlayPlane3Record **ppPlanes;
    void                               *pPostComposition;
    uint32_t                            Duration;
    uint32_t                            DurationPadding;
    void                               *pHDRMetaData;
    uint64_t                            TargetFlipTime;
};

}

#endif
