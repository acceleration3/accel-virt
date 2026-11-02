#ifndef AV_DXGKARG_QUERY_ENGINE_STATUS_RECORD_HPP
#define AV_DXGKARG_QUERY_ENGINE_STATUS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryEngineStatusRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
    uint32_t EngineStatus;
};

}

#endif
