#ifndef AV_DXGKARG_QUERY_VIDPN_HW_CAPABILITY_RECORD_HPP
#define AV_DXGKARG_QUERY_VIDPN_HW_CAPABILITY_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryVidPnHwCapabilityRecord {
    void    *hFunctionalVidPn;
    uint32_t SourceId;
    uint32_t TargetId;
    uint32_t VidPnHWCaps;
    uint32_t VidPnHWCapsPadding;
};

}

#endif
