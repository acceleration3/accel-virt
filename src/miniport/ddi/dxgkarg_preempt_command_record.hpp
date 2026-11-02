#ifndef AV_DXGKARG_PREEMPT_COMMAND_RECORD_HPP
#define AV_DXGKARG_PREEMPT_COMMAND_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargPreemptCommandRecord {
    uint32_t PreemptionFenceId;
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
    uint32_t Flags;
};

}

#endif
