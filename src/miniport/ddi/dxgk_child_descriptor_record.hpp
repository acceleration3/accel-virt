#ifndef AV_DXGK_CHILD_DESCRIPTOR_RECORD_HPP
#define AV_DXGK_CHILD_DESCRIPTOR_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkChildDescriptorRecord {
    int32_t  ChildDeviceType;
    int32_t  InterfaceTechnology;
    int32_t  MonitorOrientationAwareness;
    uint8_t  SupportsSdtvModes;
    uint8_t  Padding[3];
    int32_t  HpdAwareness;
    uint32_t AcpiUid;
    uint32_t ChildUid;
};

}

#endif
