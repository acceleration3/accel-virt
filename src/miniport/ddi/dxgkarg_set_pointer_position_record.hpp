/* DXGKARG_SETPOINTERPOSITION, as DxgkDdiSetPointerPosition receives it. A DDI
 * record: Flags is the DXGK_SETPOINTERPOSITIONFLAGS Value;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_SET_POINTER_POSITION_RECORD_HPP
#define AV_DXGKARG_SET_POINTER_POSITION_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetPointerPositionRecord {
    uint32_t VidPnSourceId;
    int32_t  X;
    int32_t  Y;
    uint32_t Flags;
};

}  // namespace av

#endif  // AV_DXGKARG_SET_POINTER_POSITION_RECORD_HPP
