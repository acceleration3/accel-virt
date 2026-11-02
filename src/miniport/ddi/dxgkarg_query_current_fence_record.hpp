#ifndef AV_DXGKARG_QUERY_CURRENT_FENCE_RECORD_HPP
#define AV_DXGKARG_QUERY_CURRENT_FENCE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryCurrentFenceRecord {
    uint32_t CurrentFence;
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
};

}

#endif
