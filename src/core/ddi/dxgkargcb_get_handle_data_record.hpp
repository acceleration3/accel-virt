/* DXGKARGCB_GETHANDLEDATA, the argument of DxgkCbGetHandleData. A DDI record:
 * Flags is the DXGKCB_GETHANDLEDATAFLAGS Value; src/km/ddi_record_layout.cpp
 * proves each offset. */
#ifndef AV_DXGKARGCB_GET_HANDLE_DATA_RECORD_HPP
#define AV_DXGKARGCB_GET_HANDLE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargcbGetHandleDataRecord {
    uint32_t hObject;
    int32_t  Type;
    uint32_t Flags;
};

}  // namespace av

#endif  // AV_DXGKARGCB_GET_HANDLE_DATA_RECORD_HPP
