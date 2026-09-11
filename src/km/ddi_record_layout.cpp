/* Proves at compile time that each DDI record in src/core/ddi/ is laid out
 * byte-for-byte as the WDK structure dxgkrnl passes, so core can be driven on
 * Linux through the same memory the kernel hands it. */
#include <ntddk.h>
#include <dispmprt.h>

#include <stddef.h>

#include "ddi/dxgk_child_descriptor_record.hpp"
#include "ddi/dxgk_child_status_record.hpp"
#include "ddi/dxgk_device_info_record.hpp"
#include "ddi/dxgk_driver_caps_record.hpp"
#include "ddi/dxgk_wddm_device_caps_record.hpp"
#include "ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "ddi/dxgkarg_escape_record.hpp"
#include "ddi/dxgkarg_query_adapter_info_record.hpp"
#include "ddi/dxgkrnl_interface_record.hpp"
#include "ddi/linked_device_record.hpp"
#include "resource_list.hpp"

#define AV_SAME_SIZE(record, wdk) \
    static_assert(sizeof(av::record) == sizeof(wdk), #record " size differs from " #wdk)
#define AV_SAME_OFFSET(record, field, wdk, wdk_field)                            \
    static_assert(offsetof(av::record, field) == offsetof(wdk, wdk_field),     \
                  #record "::" #field " is not at " #wdk "::" #wdk_field)

AV_SAME_SIZE(DxgkrnlInterfaceRecord, DXGKRNL_INTERFACE);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, Size, DXGKRNL_INTERFACE, Size);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, Version, DXGKRNL_INTERFACE, Version);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DeviceHandle, DXGKRNL_INTERFACE, DeviceHandle);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbEvalAcpiMethod, DXGKRNL_INTERFACE, DxgkCbEvalAcpiMethod);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbGetDeviceInformation, DXGKRNL_INTERFACE, DxgkCbGetDeviceInformation);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbIndicateChildStatus, DXGKRNL_INTERFACE, DxgkCbIndicateChildStatus);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbMapMemory, DXGKRNL_INTERFACE, DxgkCbMapMemory);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbQueueDpc, DXGKRNL_INTERFACE, DxgkCbQueueDpc);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbQueryServices, DXGKRNL_INTERFACE, DxgkCbQueryServices);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbReadDeviceSpace, DXGKRNL_INTERFACE, DxgkCbReadDeviceSpace);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbSynchronizeExecution, DXGKRNL_INTERFACE, DxgkCbSynchronizeExecution);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbUnmapMemory, DXGKRNL_INTERFACE, DxgkCbUnmapMemory);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbWriteDeviceSpace, DXGKRNL_INTERFACE, DxgkCbWriteDeviceSpace);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbIsDevicePresent, DXGKRNL_INTERFACE, DxgkCbIsDevicePresent);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbGetHandleData, DXGKRNL_INTERFACE, DxgkCbGetHandleData);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbGetHandleParent, DXGKRNL_INTERFACE, DxgkCbGetHandleParent);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbEnumHandleChildren, DXGKRNL_INTERFACE, DxgkCbEnumHandleChildren);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbNotifyInterrupt, DXGKRNL_INTERFACE, DxgkCbNotifyInterrupt);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbNotifyDpc, DXGKRNL_INTERFACE, DxgkCbNotifyDpc);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbQueryVidPnInterface, DXGKRNL_INTERFACE, DxgkCbQueryVidPnInterface);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbQueryMonitorInterface, DXGKRNL_INTERFACE, DxgkCbQueryMonitorInterface);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbGetCaptureAddress, DXGKRNL_INTERFACE, DxgkCbGetCaptureAddress);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbLogEtwEvent, DXGKRNL_INTERFACE, DxgkCbLogEtwEvent);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, DxgkCbExcludeAdapterAccess, DXGKRNL_INTERFACE, DxgkCbExcludeAdapterAccess);
AV_SAME_OFFSET(DxgkrnlInterfaceRecord, LaterCallbacks, DXGKRNL_INTERFACE, DxgkCbCreateContextAllocation);
static_assert(offsetof(av::DxgkrnlInterfaceRecord, LaterCallbacks) +
                      sizeof(av::DxgkrnlInterfaceRecord::LaterCallbacks) ==
                  offsetof(DXGKRNL_INTERFACE, DxgkCbDisconnectDoorbell) + sizeof(void *),
              "LaterCallbacks does not end at DxgkCbDisconnectDoorbell");

AV_SAME_SIZE(DxgkDeviceInfoRecord, DXGK_DEVICE_INFO);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, MiniportDeviceContext, DXGK_DEVICE_INFO, MiniportDeviceContext);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, PhysicalDeviceObject, DXGK_DEVICE_INFO, PhysicalDeviceObject);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, DeviceRegistryPathLength, DXGK_DEVICE_INFO, DeviceRegistryPath.Length);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, DeviceRegistryPathMaximumLength, DXGK_DEVICE_INFO, DeviceRegistryPath.MaximumLength);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, DeviceRegistryPathBuffer, DXGK_DEVICE_INFO, DeviceRegistryPath.Buffer);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, TranslatedResourceList, DXGK_DEVICE_INFO, TranslatedResourceList);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, SystemMemorySize, DXGK_DEVICE_INFO, SystemMemorySize);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, HighestPhysicalAddress, DXGK_DEVICE_INFO, HighestPhysicalAddress);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, AgpApertureBase, DXGK_DEVICE_INFO, AgpApertureBase);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, AgpApertureSize, DXGK_DEVICE_INFO, AgpApertureSize);
AV_SAME_OFFSET(DxgkDeviceInfoRecord, DockingState, DXGK_DEVICE_INFO, DockingState);

AV_SAME_SIZE(DxgkChildDescriptorRecord, DXGK_CHILD_DESCRIPTOR);
AV_SAME_OFFSET(DxgkChildDescriptorRecord, ChildDeviceType, DXGK_CHILD_DESCRIPTOR, ChildDeviceType);
AV_SAME_OFFSET(DxgkChildDescriptorRecord, InterfaceTechnology, DXGK_CHILD_DESCRIPTOR, ChildCapabilities.Type.VideoOutput.InterfaceTechnology);
AV_SAME_OFFSET(DxgkChildDescriptorRecord, MonitorOrientationAwareness, DXGK_CHILD_DESCRIPTOR, ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness);
AV_SAME_OFFSET(DxgkChildDescriptorRecord, SupportsSdtvModes, DXGK_CHILD_DESCRIPTOR, ChildCapabilities.Type.VideoOutput.SupportsSdtvModes);
AV_SAME_OFFSET(DxgkChildDescriptorRecord, HpdAwareness, DXGK_CHILD_DESCRIPTOR, ChildCapabilities.HpdAwareness);
AV_SAME_OFFSET(DxgkChildDescriptorRecord, AcpiUid, DXGK_CHILD_DESCRIPTOR, AcpiUid);
AV_SAME_OFFSET(DxgkChildDescriptorRecord, ChildUid, DXGK_CHILD_DESCRIPTOR, ChildUid);

AV_SAME_SIZE(DxgkChildStatusRecord, DXGK_CHILD_STATUS);
AV_SAME_OFFSET(DxgkChildStatusRecord, Type, DXGK_CHILD_STATUS, Type);
AV_SAME_OFFSET(DxgkChildStatusRecord, ChildUid, DXGK_CHILD_STATUS, ChildUid);
AV_SAME_OFFSET(DxgkChildStatusRecord, HotPlugConnected, DXGK_CHILD_STATUS, HotPlug.Connected);
AV_SAME_OFFSET(DxgkChildStatusRecord, MiracastMonitorType, DXGK_CHILD_STATUS, Miracast.MiracastMonitorType);

AV_SAME_SIZE(LinkedDeviceRecord, LINKED_DEVICE);
AV_SAME_OFFSET(LinkedDeviceRecord, ChainUid, LINKED_DEVICE, ChainUid);
AV_SAME_OFFSET(LinkedDeviceRecord, NumberOfLinksInChain, LINKED_DEVICE, NumberOfLinksInChain);
AV_SAME_OFFSET(LinkedDeviceRecord, LeadLink, LINKED_DEVICE, LeadLink);

AV_SAME_SIZE(DxgkargQueryAdapterInfoRecord, DXGKARG_QUERYADAPTERINFO);
AV_SAME_OFFSET(DxgkargQueryAdapterInfoRecord, Type, DXGKARG_QUERYADAPTERINFO, Type);
AV_SAME_OFFSET(DxgkargQueryAdapterInfoRecord, pInputData, DXGKARG_QUERYADAPTERINFO, pInputData);
AV_SAME_OFFSET(DxgkargQueryAdapterInfoRecord, InputDataSize, DXGKARG_QUERYADAPTERINFO, InputDataSize);
AV_SAME_OFFSET(DxgkargQueryAdapterInfoRecord, pOutputData, DXGKARG_QUERYADAPTERINFO, pOutputData);
AV_SAME_OFFSET(DxgkargQueryAdapterInfoRecord, OutputDataSize, DXGKARG_QUERYADAPTERINFO, OutputDataSize);
AV_SAME_OFFSET(DxgkargQueryAdapterInfoRecord, Flags, DXGKARG_QUERYADAPTERINFO, Flags);
AV_SAME_OFFSET(DxgkargQueryAdapterInfoRecord, hKmdProcessHandle, DXGKARG_QUERYADAPTERINFO, hKmdProcessHandle);

AV_SAME_SIZE(DxgkDriverCapsRecord, DXGK_DRIVERCAPS);
AV_SAME_OFFSET(DxgkDriverCapsRecord, HighestAcceptableAddress, DXGK_DRIVERCAPS, HighestAcceptableAddress);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxAllocationListSlotId, DXGK_DRIVERCAPS, MaxAllocationListSlotId);
AV_SAME_OFFSET(DxgkDriverCapsRecord, ApertureSegmentCommitLimit, DXGK_DRIVERCAPS, ApertureSegmentCommitLimit);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxPointerWidth, DXGK_DRIVERCAPS, MaxPointerWidth);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxPointerHeight, DXGK_DRIVERCAPS, MaxPointerHeight);
AV_SAME_OFFSET(DxgkDriverCapsRecord, PointerCaps, DXGK_DRIVERCAPS, PointerCaps);
AV_SAME_OFFSET(DxgkDriverCapsRecord, InterruptMessageNumber, DXGK_DRIVERCAPS, InterruptMessageNumber);
AV_SAME_OFFSET(DxgkDriverCapsRecord, NumberOfSwizzlingRanges, DXGK_DRIVERCAPS, NumberOfSwizzlingRanges);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxOverlays, DXGK_DRIVERCAPS, MaxOverlays);
AV_SAME_OFFSET(DxgkDriverCapsRecord, GammaRampCaps, DXGK_DRIVERCAPS, GammaRampCaps);
AV_SAME_OFFSET(DxgkDriverCapsRecord, PresentationCaps, DXGK_DRIVERCAPS, PresentationCaps);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxQueuedFlipOnVSync, DXGK_DRIVERCAPS, MaxQueuedFlipOnVSync);
AV_SAME_OFFSET(DxgkDriverCapsRecord, FlipCaps, DXGK_DRIVERCAPS, FlipCaps);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SchedulingCaps, DXGK_DRIVERCAPS, SchedulingCaps);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MemoryManagementCaps, DXGK_DRIVERCAPS, MemoryManagementCaps);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MemoryManagementCapsPagingNode, DXGK_DRIVERCAPS, MemoryManagementCaps.PagingNode);
AV_SAME_OFFSET(DxgkDriverCapsRecord, GpuEngineTopologyNbAsymetricProcessingNodes, DXGK_DRIVERCAPS, GpuEngineTopology.NbAsymetricProcessingNodes);
AV_SAME_OFFSET(DxgkDriverCapsRecord, GpuEngineTopologyReserved, DXGK_DRIVERCAPS, GpuEngineTopology.Reserved);
AV_SAME_OFFSET(DxgkDriverCapsRecord, WDDMVersion, DXGK_DRIVERCAPS, WDDMVersion);
AV_SAME_OFFSET(DxgkDriverCapsRecord, Reserved, DXGK_DRIVERCAPS, Reserved);
AV_SAME_OFFSET(DxgkDriverCapsRecord, Reserved1, DXGK_DRIVERCAPS, Reserved1);
AV_SAME_OFFSET(DxgkDriverCapsRecord, PreemptionCapsGraphicsPreemptionGranularity, DXGK_DRIVERCAPS, PreemptionCaps.GraphicsPreemptionGranularity);
AV_SAME_OFFSET(DxgkDriverCapsRecord, PreemptionCapsComputePreemptionGranularity, DXGK_DRIVERCAPS, PreemptionCaps.ComputePreemptionGranularity);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportNonVGA, DXGK_DRIVERCAPS, SupportNonVGA);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportSmoothRotation, DXGK_DRIVERCAPS, SupportSmoothRotation);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportPerEngineTDR, DXGK_DRIVERCAPS, SupportPerEngineTDR);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportDirectFlip, DXGK_DRIVERCAPS, SupportDirectFlip);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportMultiPlaneOverlay, DXGK_DRIVERCAPS, SupportMultiPlaneOverlay);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportRuntimePowerManagement, DXGK_DRIVERCAPS, SupportRuntimePowerManagement);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportSurpriseRemovalInHibernation, DXGK_DRIVERCAPS, SupportSurpriseRemovalInHibernation);
AV_SAME_OFFSET(DxgkDriverCapsRecord, HybridDiscrete, DXGK_DRIVERCAPS, HybridDiscrete);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxOverlayPlanes, DXGK_DRIVERCAPS, MaxOverlayPlanes);
AV_SAME_OFFSET(DxgkDriverCapsRecord, HybridIntegrated, DXGK_DRIVERCAPS, HybridIntegrated);
AV_SAME_OFFSET(DxgkDriverCapsRecord, InternalGpuVirtualAddressRangeStart, DXGK_DRIVERCAPS, InternalGpuVirtualAddressRangeStart);
AV_SAME_OFFSET(DxgkDriverCapsRecord, InternalGpuVirtualAddressRangeEnd, DXGK_DRIVERCAPS, InternalGpuVirtualAddressRangeEnd);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportSurpriseRemoval, DXGK_DRIVERCAPS, SupportSurpriseRemoval);
AV_SAME_OFFSET(DxgkDriverCapsRecord, SupportMultiPlaneOverlayImmediateFlip, DXGK_DRIVERCAPS, SupportMultiPlaneOverlayImmediateFlip);
AV_SAME_OFFSET(DxgkDriverCapsRecord, CursorScaledWithMultiPlaneOverlayPlane0, DXGK_DRIVERCAPS, CursorScaledWithMultiPlaneOverlayPlane0);
AV_SAME_OFFSET(DxgkDriverCapsRecord, HybridAcpiChainingRequired, DXGK_DRIVERCAPS, HybridAcpiChainingRequired);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxQueuedMultiPlaneOverlayFlipVSync, DXGK_DRIVERCAPS, MaxQueuedMultiPlaneOverlayFlipVSync);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MiscCaps, DXGK_DRIVERCAPS, MiscCaps);
AV_SAME_OFFSET(DxgkDriverCapsRecord, MaxHwQueuedFlips, DXGK_DRIVERCAPS, MaxHwQueuedFlips);
AV_SAME_OFFSET(DxgkDriverCapsRecord, HwQueuedFlipCaps, DXGK_DRIVERCAPS, HwQueuedFlipCaps);
static_assert(DXGKDDI_WDDMv1 == 0x1000 && DXGKQAITYPE_DRIVERCAPS == 1 && DXGKQAITYPE_WDDMDEVICECAPS == 29 &&
                  DXGKQAITYPE_PAGINGPROCESSGPUVASIZE == 48,
              "WDDM version and query types");

AV_SAME_SIZE(DxgkWddmDeviceCapsRecord, DXGK_WDDMDEVICECAPS);
AV_SAME_OFFSET(DxgkWddmDeviceCapsRecord, WDDMVersion, DXGK_WDDMDEVICECAPS, WDDMVersion);
static_assert(sizeof(DXGK_WDDMDEVICECAPSIN) == sizeof(uint32_t), "DXGK_WDDMDEVICECAPSIN");

AV_SAME_SIZE(DxgkargEscapeRecord, DXGKARG_ESCAPE);
AV_SAME_OFFSET(DxgkargEscapeRecord, hDevice, DXGKARG_ESCAPE, hDevice);
AV_SAME_OFFSET(DxgkargEscapeRecord, Flags, DXGKARG_ESCAPE, Flags);
AV_SAME_OFFSET(DxgkargEscapeRecord, pPrivateDriverData, DXGKARG_ESCAPE, pPrivateDriverData);
AV_SAME_OFFSET(DxgkargEscapeRecord, PrivateDriverDataSize, DXGKARG_ESCAPE, PrivateDriverDataSize);
AV_SAME_OFFSET(DxgkargEscapeRecord, hContext, DXGKARG_ESCAPE, hContext);
AV_SAME_OFFSET(DxgkargEscapeRecord, hKmdProcessHandle, DXGKARG_ESCAPE, hKmdProcessHandle);

AV_SAME_SIZE(DxgkargCollectDbgInfoRecord, DXGKARG_COLLECTDBGINFO);
AV_SAME_OFFSET(DxgkargCollectDbgInfoRecord, Reason, DXGKARG_COLLECTDBGINFO, Reason);
AV_SAME_OFFSET(DxgkargCollectDbgInfoRecord, pBuffer, DXGKARG_COLLECTDBGINFO, pBuffer);
AV_SAME_OFFSET(DxgkargCollectDbgInfoRecord, BufferSize, DXGKARG_COLLECTDBGINFO, BufferSize);
AV_SAME_OFFSET(DxgkargCollectDbgInfoRecord, pExtension, DXGKARG_COLLECTDBGINFO, pExtension);

AV_SAME_SIZE(DxgkargCollectDbgInfoExtRecord, DXGKARG_COLLECTDBGINFO_EXT);
AV_SAME_OFFSET(DxgkargCollectDbgInfoExtRecord, BucketingKey, DXGKARG_COLLECTDBGINFO_EXT, BucketingKey);
AV_SAME_OFFSET(DxgkargCollectDbgInfoExtRecord, CurrentDmaBufferOffset, DXGKARG_COLLECTDBGINFO_EXT, CurrentDmaBufferOffset);
AV_SAME_OFFSET(DxgkargCollectDbgInfoExtRecord, Reserved7, DXGKARG_COLLECTDBGINFO_EXT, Reserved7);

/* ResourceList reads CM_RESOURCE_LIST as bytes; these pin the offsets it reads at. */
static_assert(offsetof(CM_RESOURCE_LIST, List) == av::ResourceList::list_header_bytes, "CM_RESOURCE_LIST::List");
static_assert(offsetof(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList.PartialDescriptors) ==
                  av::ResourceList::full_header_bytes, "PartialDescriptors");
static_assert(offsetof(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList.Count) ==
                  av::ResourceList::partial_count_offset, "PartialResourceList.Count");
static_assert(sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) == av::ResourceList::partial_descriptor_bytes,
              "CM_PARTIAL_RESOURCE_DESCRIPTOR size");
static_assert(offsetof(CM_PARTIAL_RESOURCE_DESCRIPTOR, Flags) == av::ResourceList::partial_flags_offset,
              "CM_PARTIAL_RESOURCE_DESCRIPTOR::Flags");
static_assert(offsetof(CM_PARTIAL_RESOURCE_DESCRIPTOR, u.Memory.Start) == av::ResourceList::memory_start_offset,
              "u.Memory.Start");
static_assert(offsetof(CM_PARTIAL_RESOURCE_DESCRIPTOR, u.Memory.Length) == av::ResourceList::memory_length_offset,
              "u.Memory.Length");
static_assert(offsetof(CM_PARTIAL_RESOURCE_DESCRIPTOR, u.Memory40.Length40) == av::ResourceList::memory_length_offset,
              "u.Memory40.Length40");
static_assert(offsetof(CM_PARTIAL_RESOURCE_DESCRIPTOR, u.Memory64.Length64) == av::ResourceList::memory_length_offset,
              "u.Memory64.Length64");
static_assert(CmResourceTypeMemory == 3 && CmResourceTypeMemoryLarge == 7 && CmResourceTypeInterrupt == 2,
              "resource types");
static_assert(CM_RESOURCE_INTERRUPT_MESSAGE == 0x02 && CM_RESOURCE_MEMORY_LARGE == 0x0E00 &&
                  CM_RESOURCE_MEMORY_LARGE_40 == 0x0200 && CM_RESOURCE_MEMORY_LARGE_48 == 0x0400 &&
                  CM_RESOURCE_MEMORY_LARGE_64 == 0x0800,
              "resource flags");
