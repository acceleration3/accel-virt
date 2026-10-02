/* DXGKARG_GETSCANLINE, as DxgkDdiGetScanLine receives and answers it. A DDI
 * record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_GET_SCAN_LINE_RECORD_HPP
#define AV_DXGKARG_GET_SCAN_LINE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargGetScanLineRecord {
    uint32_t VidPnTargetId;
    uint8_t  InVerticalBlank;
    uint8_t  Padding[3];
    uint32_t ScanLine;
};

}  // namespace av

#endif  // AV_DXGKARG_GET_SCAN_LINE_RECORD_HPP
