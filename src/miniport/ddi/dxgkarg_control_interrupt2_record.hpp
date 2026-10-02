/* DXGKARG_CONTROLINTERRUPT2, which DxgkDdiControlInterrupt2 receives by value. A
 * DDI record: the state union is its DXGK_CRTC_VSYNC_STATE member;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_CONTROL_INTERRUPT2_RECORD_HPP
#define AV_DXGKARG_CONTROL_INTERRUPT2_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargControlInterrupt2Record {
    int32_t InterruptType;
    int32_t CrtcVsyncState;
};

}  // namespace av

#endif  // AV_DXGKARG_CONTROL_INTERRUPT2_RECORD_HPP
