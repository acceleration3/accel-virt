/* DXGKARG_QUERYCURRENTFENCE, as DxgkDdiQueryCurrentFence receives and answers
 * it. A DDI record. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_QUERY_CURRENT_FENCE_RECORD_HPP
#define AV_DXGKARG_QUERY_CURRENT_FENCE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryCurrentFenceRecord {
    uint32_t CurrentFence;
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
};

}  // namespace av

#endif  // AV_DXGKARG_QUERY_CURRENT_FENCE_RECORD_HPP
