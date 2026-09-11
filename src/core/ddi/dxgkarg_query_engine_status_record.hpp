/* DXGKARG_QUERYENGINESTATUS, as DxgkDdiQueryEngineStatus receives and answers
 * it. A DDI record: EngineStatus is the DXGK_ENGINESTATUS Value;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_QUERY_ENGINE_STATUS_RECORD_HPP
#define AV_DXGKARG_QUERY_ENGINE_STATUS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryEngineStatusRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
    uint32_t EngineStatus;
};

}  // namespace av

#endif  // AV_DXGKARG_QUERY_ENGINE_STATUS_RECORD_HPP
