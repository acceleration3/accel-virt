/* DXGK_CHILD_STATUS, as DxgkDdiQueryChildStatus reads and answers it. A DDI
 * record: the union is its HotPlug.Connected byte and Miracast's monitor type;
 * src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGK_CHILD_STATUS_RECORD_HPP
