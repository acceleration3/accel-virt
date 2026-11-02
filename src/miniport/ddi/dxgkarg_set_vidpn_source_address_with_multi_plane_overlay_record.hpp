#ifndef AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_WITH_MULTI_PLANE_OVERLAY_RECORD_HPP
#define AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_WITH_MULTI_PLANE_OVERLAY_RECORD_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord {
    uint32_t                          ContextCount;
    uint32_t                          ContextCountPadding;
    void                             *Context[65];
    uint32_t                          Flags;
    uint32_t                          VidPnSourceId;
    uint32_t                          PlaneCount;
    uint32_t                          PlaneCountPadding;
    DxgkMultiplaneOverlayPlaneRecord *pPlanes;
    uint32_t                          Duration;
    uint32_t                          DurationPadding;
};

}

#endif
