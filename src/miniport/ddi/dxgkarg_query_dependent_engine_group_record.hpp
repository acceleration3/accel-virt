#ifndef AV_DXGKARG_QUERY_DEPENDENT_ENGINE_GROUP_RECORD_HPP
#define AV_DXGKARG_QUERY_DEPENDENT_ENGINE_GROUP_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryDependentEngineGroupRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
    uint64_t DependentNodeOrdinalMask;
};

}

#endif
