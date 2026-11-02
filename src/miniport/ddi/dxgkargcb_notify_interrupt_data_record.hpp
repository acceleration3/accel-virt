#ifndef AV_DXGKARGCB_NOTIFY_INTERRUPT_DATA_RECORD_HPP
#define AV_DXGKARGCB_NOTIFY_INTERRUPT_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargcbNotifyInterruptDataRecord {
    int32_t  InterruptType;
    uint32_t InterruptTypePadding;
    uint32_t FenceId;
    uint32_t Argument1;
    uint32_t Argument2;
    uint32_t Argument3;
    uint32_t UnionRemainder[12];
    uint32_t Flags;
    uint32_t Padding;
};

}

#endif
