/* DXGKARG_CONTROLMODEBEHAVIOR, as DxgkDdiControlModeBehavior receives and
 * answers it. A DDI record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_CONTROL_MODE_BEHAVIOR_RECORD_HPP
#define AV_DXGKARG_CONTROL_MODE_BEHAVIOR_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargControlModeBehaviorRecord {
    uint32_t Request;
    uint32_t Satisfied;
    uint32_t NotSatisfied;
};

}  // namespace av

#endif  // AV_DXGKARG_CONTROL_MODE_BEHAVIOR_RECORD_HPP
