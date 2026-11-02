#ifndef AV_DXGKARG_IS_SUPPORTED_VIDPN_RECORD_HPP
#define AV_DXGKARG_IS_SUPPORTED_VIDPN_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargIsSupportedVidPnRecord {
    void    *hDesiredVidPn;
    uint8_t IsVidPnSupported;
    uint8_t Padding[7];
};

}

#endif
