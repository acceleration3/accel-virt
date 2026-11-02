#ifndef AV_DXGKARG_RESET_HW_ENGINE_RECORD_HPP
#define AV_DXGKARG_RESET_HW_ENGINE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargResetHwEngineRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
};

}

#endif
