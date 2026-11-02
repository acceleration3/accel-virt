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
    const uint8_t *TranslatedResourceList;
    int64_t        SystemMemorySize;
    int64_t        HighestPhysicalAddress;
    int64_t        AgpApertureBase;
    uint64_t       AgpApertureSize;
    int32_t        DockingState;
    uint32_t       Padding;
};

}

#endif
