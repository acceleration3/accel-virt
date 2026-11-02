#ifndef AV_DXGKARG_SET_VIDPN_SOURCE_VISIBILITY_RECORD_HPP
#define AV_DXGKARG_SET_VIDPN_SOURCE_VISIBILITY_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetVidPnSourceVisibilityRecord {
    uint32_t VidPnSourceId;
    uint8_t  Visible;
    uint8_t  VisiblePadding[3];
};

}

#endif
