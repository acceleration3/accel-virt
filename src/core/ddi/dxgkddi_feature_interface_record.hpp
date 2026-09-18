/* The interface dxgkrnl asks a driver for when it wants to know which features
 * the driver implements (dispmprt.h:1586, WDDM 3.2).
 *
 * `DxgkDdiQueryInterface` is asked for `GUID_WDDM_INTERFACE_FEATURE` and fills
 * this table in the caller's memory; dxgkrnl then calls `QueryFeatureSupport`
 * once per feature it is staging. A feature the driver does not answer for is
 * one the OS leaves disabled - and a capability behind a disabled feature
 * fails the adapter start if the driver reports it anyway (feature 049,
 * research.md §18).
 *
 * A DDI record; src/km/ddi_record_layout.cpp proves each offset. */
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

/* DXGKARG_QUERYFEATURESUPPORT (d3dkmddi.h:10746): which feature, and what the
 * driver answers about it. */
struct DxgkargQueryFeatureSupportRecord {
    uint32_t FeatureId;
    uint16_t MinSupportedVersion;
    uint16_t MaxSupportedVersion;
    uint8_t  AllowExperimental;
    uint8_t  SupportedByDriver;
    uint8_t  SupportedOnCurrentConfig;
};

/* The versions a driver says it implements. dxgkrnl intersects the driver's
 * range with its own and enables the feature at the highest version in both
 * (DpiIsFeatureEnabled), so a range narrower than the OS's staged version
 * disables the feature outright - which is what a first live run showed
 * (feature 049, research.md §18). This driver implements each feature as the
 * WDDM 3.2 headers define its mechanism and has no revisions of its own, so
 * what it reports is every version: the mechanism is the same one whichever
 * number this OS staged it under. */
enum : uint16_t { feature_version_lowest = 0u, feature_version_highest = 0xFFFFu };
/* DXGK_FEATURE_INTERFACE_VERSION_1 (dispmprt.h:1572). */
enum : uint16_t { feature_interface_version_1 = 1u };

/* QUERY_INTERFACE (video.h:1048), as DxgkDdiQueryInterface receives it. */
struct QueryInterfaceRecord {
    const void *InterfaceType;
    uint16_t    Size;
    uint16_t    Version;
    void       *Interface;
    void       *InterfaceSpecificData;
    uint32_t    DeviceUid;
};

}  // namespace av

#endif  // AV_DXGKDDI_FEATURE_INTERFACE_RECORD_HPP
