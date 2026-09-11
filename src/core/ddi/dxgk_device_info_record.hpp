/* DXGK_DEVICE_INFO, as DxgkCbGetDeviceInformation fills it. A DDI record:
 * fields keep the WDK's names and src/km/ddi_record_layout.cpp proves each
 * offset. DeviceRegistryPath is the UNICODE_STRING's three fields. */
#ifndef AV_DXGK_DEVICE_INFO_RECORD_HPP
#define AV_DXGK_DEVICE_INFO_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkDeviceInfoRecord {
    void          *MiniportDeviceContext;
    void          *PhysicalDeviceObject;
    uint16_t       DeviceRegistryPathLength;
    uint16_t       DeviceRegistryPathMaximumLength;
    uint32_t       DeviceRegistryPathPadding;
    void          *DeviceRegistryPathBuffer;
    /* A CM_RESOURCE_LIST; ResourceList reads it as bytes. */
    const uint8_t *TranslatedResourceList;
    int64_t        SystemMemorySize;
    int64_t        HighestPhysicalAddress;
    int64_t        AgpApertureBase;
    uint64_t       AgpApertureSize;
    int32_t        DockingState;
    uint32_t       Padding;
};

}  // namespace av

#endif  // AV_DXGK_DEVICE_INFO_RECORD_HPP
