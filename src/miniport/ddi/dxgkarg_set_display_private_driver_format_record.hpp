#ifndef AV_DXGKARG_SET_DISPLAY_PRIVATE_DRIVER_FORMAT_RECORD_HPP
#define AV_DXGKARG_SET_DISPLAY_PRIVATE_DRIVER_FORMAT_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetDisplayPrivateDriverFormatRecord {
    uint32_t VidPnSourceId;
    uint32_t VidPnSourceIdPadding;
    void     *PrimaryAllocation;
    uint32_t PrivateDriverFormatAttribute;
    uint32_t Padding;
};

}

#endif
