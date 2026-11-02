#ifndef AV_DXGK_CHILD_STATUS_RECORD_HPP
#define AV_DXGK_CHILD_STATUS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkChildStatusRecord {
    int32_t  Type;
    uint32_t ChildUid;
    uint8_t  HotPlugConnected;
    uint8_t  Padding[3];
    int32_t  MiracastMonitorType;
};

}

#endif
