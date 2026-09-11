/* The interface tables dxgkrnl hands a driver for a VidPn - DXGK_VIDPN_INTERFACE,
 * DXGK_VIDPNTOPOLOGY_INTERFACE, DXGK_VIDPNSOURCEMODESET_INTERFACE,
 * DXGK_VIDPNTARGETMODESET_INTERFACE - and for a monitor's source modes,
 * DXGK_MONITORSOURCEMODESET_INTERFACE. DDI records of typed function pointers,
 * kept together because each table names the others; src/km/ddi_record_layout.cpp
 * proves each offset. Handles are void *, SIZE_T uint64_t, NTSTATUS int32_t. */
#ifndef AV_DXGK_VIDPN_INTERFACE_RECORDS_HPP
#define AV_DXGK_VIDPN_INTERFACE_RECORDS_HPP

#include "d3dkmdt_monitor_source_mode_record.hpp"
#include "d3dkmdt_vidpn_present_path_record.hpp"
#include "d3dkmdt_vidpn_source_mode_record.hpp"
#include "d3dkmdt_vidpn_target_mode_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkVidPnTopologyInterfaceRecord;
struct DxgkVidPnSourceModeSetInterfaceRecord;
struct DxgkVidPnTargetModeSetInterfaceRecord;

using VidPnGetTopology = int32_t (*)(void *vidpn, void **topology,
                                     const DxgkVidPnTopologyInterfaceRecord **topology_interface);
using VidPnAcquireSourceModeSet = int32_t (*)(void *vidpn, uint32_t source, void **mode_set,
                                              const DxgkVidPnSourceModeSetInterfaceRecord **mode_set_interface);
using VidPnAcquireTargetModeSet = int32_t (*)(void *vidpn, uint32_t target, void **mode_set,
                                              const DxgkVidPnTargetModeSetInterfaceRecord **mode_set_interface);
using VidPnReleaseModeSet = int32_t (*)(void *vidpn, void *mode_set);
using VidPnAssignModeSet = int32_t (*)(void *vidpn, uint32_t owner, void *mode_set);

using TopologyCount = int32_t (*)(void *topology, uint64_t *paths);
using TopologyCountFromSource = int32_t (*)(void *topology, uint32_t source, uint64_t *paths);
using TopologyEnumTargets = int32_t (*)(void *topology, uint32_t source, uint64_t index, uint32_t *target);
using TopologySourceFromTarget = int32_t (*)(void *topology, uint32_t target, uint32_t *source);
using TopologyAcquirePath = int32_t (*)(void *topology, uint32_t source, uint32_t target,
                                        const D3dkmdtVidPnPresentPathRecord **path);
using TopologyAcquireFirstPath = int32_t (*)(void *topology, const D3dkmdtVidPnPresentPathRecord **path);
using TopologyAcquireNextPath = int32_t (*)(void *topology, const D3dkmdtVidPnPresentPathRecord *path,
                                            const D3dkmdtVidPnPresentPathRecord **next);
using TopologyPathOperation = int32_t (*)(void *topology, const D3dkmdtVidPnPresentPathRecord *path);

using SourceModeSetCount = int32_t (*)(void *mode_set, uint64_t *modes);
using SourceModeSetAcquire = int32_t (*)(void *mode_set, const D3dkmdtVidPnSourceModeRecord **mode);
using SourceModeSetAcquireNext = int32_t (*)(void *mode_set, const D3dkmdtVidPnSourceModeRecord *mode,
                                             const D3dkmdtVidPnSourceModeRecord **next);
using SourceModeSetOperation = int32_t (*)(void *mode_set, const D3dkmdtVidPnSourceModeRecord *mode);
using SourceModeSetCreate = int32_t (*)(void *mode_set, D3dkmdtVidPnSourceModeRecord **mode);
using ModeSetPin = int32_t (*)(void *mode_set, uint32_t mode_id);

using TargetModeSetAcquire = int32_t (*)(void *mode_set, const D3dkmdtVidPnTargetModeRecord **mode);
using TargetModeSetAcquireNext = int32_t (*)(void *mode_set, const D3dkmdtVidPnTargetModeRecord *mode,
                                             const D3dkmdtVidPnTargetModeRecord **next);
using TargetModeSetOperation = int32_t (*)(void *mode_set, const D3dkmdtVidPnTargetModeRecord *mode);
using TargetModeSetCreate = int32_t (*)(void *mode_set, D3dkmdtVidPnTargetModeRecord **mode);

using MonitorModeSetCreate = int32_t (*)(void *mode_set, D3dkmdtMonitorSourceModeRecord **mode);
using MonitorModeSetOperation = int32_t (*)(void *mode_set, const D3dkmdtMonitorSourceModeRecord *mode);

struct DxgkVidPnInterfaceRecord {
    int32_t                   Version;
    uint32_t                  VersionPadding;
    VidPnGetTopology          pfnGetTopology;
    VidPnAcquireSourceModeSet pfnAcquireSourceModeSet;
    VidPnReleaseModeSet       pfnReleaseSourceModeSet;
    VidPnAcquireSourceModeSet pfnCreateNewSourceModeSet;
    VidPnAssignModeSet        pfnAssignSourceModeSet;
    void                     *pfnAssignMultisamplingMethodSet;
    VidPnAcquireTargetModeSet pfnAcquireTargetModeSet;
    VidPnReleaseModeSet       pfnReleaseTargetModeSet;
    VidPnAcquireTargetModeSet pfnCreateNewTargetModeSet;
    VidPnAssignModeSet        pfnAssignTargetModeSet;
};

struct DxgkVidPnTopologyInterfaceRecord {
    TopologyCount            pfnGetNumPaths;
    TopologyCountFromSource  pfnGetNumPathsFromSource;
    TopologyEnumTargets      pfnEnumPathTargetsFromSource;
    TopologySourceFromTarget pfnGetPathSourceFromTarget;
    TopologyAcquirePath      pfnAcquirePathInfo;
    TopologyAcquireFirstPath pfnAcquireFirstPathInfo;
    TopologyAcquireNextPath  pfnAcquireNextPathInfo;
    TopologyPathOperation    pfnUpdatePathSupportInfo;
    TopologyPathOperation    pfnReleasePathInfo;
    void                    *pfnCreateNewPathInfo;
    void                    *pfnAddPath;
    void                    *pfnRemovePath;
};

struct DxgkVidPnSourceModeSetInterfaceRecord {
    SourceModeSetCount       pfnGetNumModes;
    SourceModeSetAcquire     pfnAcquireFirstModeInfo;
    SourceModeSetAcquireNext pfnAcquireNextModeInfo;
    SourceModeSetAcquire     pfnAcquirePinnedModeInfo;
    SourceModeSetOperation   pfnReleaseModeInfo;
    SourceModeSetCreate      pfnCreateNewModeInfo;
    SourceModeSetOperation   pfnAddMode;
    ModeSetPin               pfnPinMode;
};

struct DxgkVidPnTargetModeSetInterfaceRecord {
    SourceModeSetCount       pfnGetNumModes;
    TargetModeSetAcquire     pfnAcquireFirstModeInfo;
    TargetModeSetAcquireNext pfnAcquireNextModeInfo;
    TargetModeSetAcquire     pfnAcquirePinnedModeInfo;
    TargetModeSetOperation   pfnReleaseModeInfo;
    TargetModeSetCreate      pfnCreateNewModeInfo;
    TargetModeSetOperation   pfnAddMode;
    ModeSetPin               pfnPinMode;
};

struct DxgkMonitorSourceModeSetInterfaceRecord {
    SourceModeSetCount      pfnGetNumModes;
    void                   *pfnAcquirePreferredModeInfo;
    void                   *pfnAcquireFirstModeInfo;
    void                   *pfnAcquireNextModeInfo;
    MonitorModeSetCreate    pfnCreateNewModeInfo;
    MonitorModeSetOperation pfnAddMode;
    MonitorModeSetOperation pfnReleaseModeInfo;
};

}  // namespace av

#endif  // AV_DXGK_VIDPN_INTERFACE_RECORDS_HPP
