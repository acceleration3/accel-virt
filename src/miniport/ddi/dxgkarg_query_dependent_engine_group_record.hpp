/* DXGKARG_QUERYDEPENDENTENGINEGROUP, as DxgkDdiQueryDependentEngineGroup
 * receives and answers it. A DDI record; src/km/ddi_record_layout.cpp proves
 * each offset. */
#ifndef AV_DXGKARG_QUERY_DEPENDENT_ENGINE_GROUP_RECORD_HPP
#define AV_DXGKARG_QUERY_DEPENDENT_ENGINE_GROUP_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryDependentEngineGroupRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
    uint64_t DependentNodeOrdinalMask;
};

}  // namespace av

#endif  // AV_DXGKARG_QUERY_DEPENDENT_ENGINE_GROUP_RECORD_HPP
