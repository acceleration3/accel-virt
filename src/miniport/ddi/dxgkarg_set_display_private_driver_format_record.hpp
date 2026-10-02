/* DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT, as
 * DxgkDdiSetDisplayPrivateDriverFormat receives it. A DDI record.
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_SET_DISPLAY_PRIVATE_DRIVER_FORMAT_RECORD_HPP
#define AV_DXGKARG_SET_DISPLAY_PRIVATE_DRIVER_FORMAT_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetDisplayPrivateDriverFormatRecord {
    uint32_t VidPnSourceId;
    uint32_t VidPnSourceIdPadding;
    void     *PrimaryAllocation;
    uint32_t PrivateDriverFormatAttribute;
    uint32_t Padding;
};

}  // namespace av

#endif  // AV_DXGKARG_SET_DISPLAY_PRIVATE_DRIVER_FORMAT_RECORD_HPP
