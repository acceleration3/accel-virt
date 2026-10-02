/* DXGK_CHILD_DESCRIPTOR, one element of the array DxgkDdiQueryChildRelations
 * fills. A DDI record: ChildCapabilities is flattened to its VideoOutput and
 * HpdAwareness fields; src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGK_CHILD_DESCRIPTOR_RECORD_HPP
