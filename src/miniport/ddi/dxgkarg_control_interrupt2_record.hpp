#ifndef AV_DXGKARG_CONTROL_INTERRUPT2_RECORD_HPP
#define AV_DXGKARG_CONTROL_INTERRUPT2_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargControlInterrupt2Record {
    int32_t InterruptType;
    int32_t CrtcVsyncState;
};

}

#endif
