#ifndef AV_DXGKARG_RESUME_HW_ENGINE_RECORD_HPP
#define AV_DXGKARG_RESUME_HW_ENGINE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargResumeHwEngineRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
};

}

#endif
