/* DXGKARG_RESETENGINE, as DxgkDdiResetEngine receives and answers it. A DDI
 * record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_RESET_ENGINE_RECORD_HPP
#define AV_DXGKARG_RESET_ENGINE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargResetEngineRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
    uint32_t LastAbortedFenceId;
};

}  // namespace av

#endif  // AV_DXGKARG_RESET_ENGINE_RECORD_HPP
