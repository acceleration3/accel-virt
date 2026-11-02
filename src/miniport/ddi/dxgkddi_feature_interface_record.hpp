#ifndef AV_DXGKDDI_FEATURE_INTERFACE_RECORD_HPP
#define AV_DXGKDDI_FEATURE_INTERFACE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryFeatureSupportRecord;

using DxgkDdiQueryFeatureSupport = int32_t (*)(void *adapter, DxgkargQueryFeatureSupportRecord *arguments);
using DxgkDdiQueryFeatureInterface = int32_t (*)(void *adapter, void *arguments);
using DxgkInterfaceReference = void (*)(void *context);
using DxgkInterfaceDereference = void (*)(void *context);

struct DxgkddiFeatureInterfaceRecord {
    uint16_t                     Size;
    uint16_t                     Version;
    void                        *Context;
    DxgkInterfaceReference       InterfaceReference;
    DxgkInterfaceDereference     InterfaceDereference;
    DxgkDdiQueryFeatureSupport   QueryFeatureSupport;
    DxgkDdiQueryFeatureInterface QueryFeatureInterface;
};

struct DxgkargQueryFeatureSupportRecord {
    uint32_t FeatureId;
    uint16_t MinSupportedVersion;
    uint16_t MaxSupportedVersion;
    uint8_t  AllowExperimental;
    uint8_t  SupportedByDriver;
    uint8_t  SupportedOnCurrentConfig;
};

enum : uint16_t { feature_version_lowest = 0u, feature_version_highest = 0xFFFFu };
enum : uint16_t { feature_interface_version_1 = 1u };

struct QueryInterfaceRecord {
    const void *InterfaceType;
    uint16_t    Size;
    uint16_t    Version;
    void       *Interface;
    void       *InterfaceSpecificData;
    uint32_t    DeviceUid;
};

}

#endif
