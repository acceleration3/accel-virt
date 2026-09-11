/* DXGKARG_SETSTABLEPOWERSTATE, as DxgkDdiSetStablePowerState receives it. A DDI
 * record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_SET_STABLE_POWER_STATE_RECORD_HPP
#define AV_DXGKARG_SET_STABLE_POWER_STATE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetStablePowerStateRecord {
    int32_t Enabled;
};

}  // namespace av

#endif  // AV_DXGKARG_SET_STABLE_POWER_STATE_RECORD_HPP
