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

}

#endif
