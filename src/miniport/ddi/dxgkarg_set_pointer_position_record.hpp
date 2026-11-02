#ifndef AV_DXGKARG_SET_POINTER_POSITION_RECORD_HPP
#define AV_DXGKARG_SET_POINTER_POSITION_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetPointerPositionRecord {
    uint32_t VidPnSourceId;
    int32_t  X;
    int32_t  Y;
    uint32_t Flags;
};

}

#endif
