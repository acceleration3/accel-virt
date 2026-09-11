/* Proves at compile time that each DDI record in src/core/ddi/ is laid out
 * byte-for-byte as the WDK structure dxgkrnl passes, so core can be driven on
 * Linux through the same memory the kernel hands it. */
#include <ntddk.h>
#include <dispmprt.h>

#include <stddef.h>

#include "ddi/d3dkmdt_gdi_surface_data_record.hpp"
#include "ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "ddi/d3dkmdt_shared_primary_surface_data_record.hpp"
#include "ddi/d3dkmdt_staging_surface_data_record.hpp"
#include "ddi/dxgk_allocation_info_record.hpp"
#include "ddi/dxgk_open_allocation_info_record.hpp"
#include "ddi/dxgk_query_segment_out4_record.hpp"
#include "ddi/dxgk_segment_descriptor4_record.hpp"
#include "ddi/dxgkarg_close_allocation_record.hpp"
#include "ddi/dxgkarg_create_allocation_record.hpp"
#include "ddi/dxgkarg_create_device_record.hpp"
#include "ddi/dxgkarg_describe_allocation_record.hpp"
#include "ddi/dxgkarg_destroy_allocation_record.hpp"
#include "ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "ddi/dxgkarg_open_allocation_record.hpp"
#include "ddi/dxgkarg_set_display_private_driver_format_record.hpp"
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

AV_SAME_SIZE(DxgkargCreateDeviceRecord, DXGKARG_CREATEDEVICE);
AV_SAME_OFFSET(DxgkargCreateDeviceRecord, hDevice, DXGKARG_CREATEDEVICE, hDevice);
AV_SAME_OFFSET(DxgkargCreateDeviceRecord, Flags, DXGKARG_CREATEDEVICE, Flags);
AV_SAME_OFFSET(DxgkargCreateDeviceRecord, Pasid, DXGKARG_CREATEDEVICE, Pasid);
AV_SAME_OFFSET(DxgkargCreateDeviceRecord, hKmdProcess, DXGKARG_CREATEDEVICE, hKmdProcess);

AV_SAME_SIZE(DxgkAllocationInfoRecord, DXGK_ALLOCATIONINFO);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, pPrivateDriverData, DXGK_ALLOCATIONINFO, pPrivateDriverData);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, PrivateDriverDataSize, DXGK_ALLOCATIONINFO, PrivateDriverDataSize);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, Alignment, DXGK_ALLOCATIONINFO, Alignment);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, Size, DXGK_ALLOCATIONINFO, Size);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, PitchAlignedSize, DXGK_ALLOCATIONINFO, PitchAlignedSize);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, HintedBank, DXGK_ALLOCATIONINFO, HintedBank);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, PreferredSegment, DXGK_ALLOCATIONINFO, PreferredSegment);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, SupportedReadSegmentSet, DXGK_ALLOCATIONINFO, SupportedReadSegmentSet);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, SupportedWriteSegmentSet, DXGK_ALLOCATIONINFO, SupportedWriteSegmentSet);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, EvictionSegmentSet, DXGK_ALLOCATIONINFO, EvictionSegmentSet);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, MaximumRenamingListLength, DXGK_ALLOCATIONINFO, MaximumRenamingListLength);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, hAllocation, DXGK_ALLOCATIONINFO, hAllocation);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, Flags, DXGK_ALLOCATIONINFO, Flags);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, pAllocationUsageHint, DXGK_ALLOCATIONINFO, pAllocationUsageHint);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, AllocationPriority, DXGK_ALLOCATIONINFO, AllocationPriority);
AV_SAME_OFFSET(DxgkAllocationInfoRecord, Flags2, DXGK_ALLOCATIONINFO, Flags2);

AV_SAME_SIZE(DxgkargCreateAllocationRecord, DXGKARG_CREATEALLOCATION);
AV_SAME_OFFSET(DxgkargCreateAllocationRecord, pPrivateDriverData, DXGKARG_CREATEALLOCATION, pPrivateDriverData);
AV_SAME_OFFSET(DxgkargCreateAllocationRecord, PrivateDriverDataSize, DXGKARG_CREATEALLOCATION, PrivateDriverDataSize);
AV_SAME_OFFSET(DxgkargCreateAllocationRecord, NumAllocations, DXGKARG_CREATEALLOCATION, NumAllocations);
AV_SAME_OFFSET(DxgkargCreateAllocationRecord, pAllocationInfo, DXGKARG_CREATEALLOCATION, pAllocationInfo);
AV_SAME_OFFSET(DxgkargCreateAllocationRecord, hResource, DXGKARG_CREATEALLOCATION, hResource);
AV_SAME_OFFSET(DxgkargCreateAllocationRecord, Flags, DXGKARG_CREATEALLOCATION, Flags);

AV_SAME_SIZE(DxgkargDestroyAllocationRecord, DXGKARG_DESTROYALLOCATION);
AV_SAME_OFFSET(DxgkargDestroyAllocationRecord, NumAllocations, DXGKARG_DESTROYALLOCATION, NumAllocations);
AV_SAME_OFFSET(DxgkargDestroyAllocationRecord, pAllocationList, DXGKARG_DESTROYALLOCATION, pAllocationList);
AV_SAME_OFFSET(DxgkargDestroyAllocationRecord, hResource, DXGKARG_DESTROYALLOCATION, hResource);
AV_SAME_OFFSET(DxgkargDestroyAllocationRecord, Flags, DXGKARG_DESTROYALLOCATION, Flags);

AV_SAME_SIZE(DxgkargDescribeAllocationRecord, DXGKARG_DESCRIBEALLOCATION);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, hAllocation, DXGKARG_DESCRIBEALLOCATION, hAllocation);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, Width, DXGKARG_DESCRIBEALLOCATION, Width);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, Height, DXGKARG_DESCRIBEALLOCATION, Height);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, Format, DXGKARG_DESCRIBEALLOCATION, Format);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, MultisampleMethodType, DXGKARG_DESCRIBEALLOCATION, MultisampleMethod.NumSamples);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, MultisampleMethodQuality, DXGKARG_DESCRIBEALLOCATION, MultisampleMethod.NumQualityLevels);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, RefreshRateNumerator, DXGKARG_DESCRIBEALLOCATION, RefreshRate.Numerator);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, RefreshRateDenominator, DXGKARG_DESCRIBEALLOCATION, RefreshRate.Denominator);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, PrivateDriverFormatAttribute, DXGKARG_DESCRIBEALLOCATION, PrivateDriverFormatAttribute);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, Flags, DXGKARG_DESCRIBEALLOCATION, Flags);
AV_SAME_OFFSET(DxgkargDescribeAllocationRecord, Rotation, DXGKARG_DESCRIBEALLOCATION, Rotation);

AV_SAME_SIZE(DxgkargGetStandardAllocationDriverDataRecord, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA);
AV_SAME_OFFSET(DxgkargGetStandardAllocationDriverDataRecord, StandardAllocationType, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, StandardAllocationType);
AV_SAME_OFFSET(DxgkargGetStandardAllocationDriverDataRecord, pCreateSurfaceData, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, pCreateSharedPrimarySurfaceData);
AV_SAME_OFFSET(DxgkargGetStandardAllocationDriverDataRecord, pAllocationPrivateDriverData, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, pAllocationPrivateDriverData);
AV_SAME_OFFSET(DxgkargGetStandardAllocationDriverDataRecord, AllocationPrivateDriverDataSize, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, AllocationPrivateDriverDataSize);
AV_SAME_OFFSET(DxgkargGetStandardAllocationDriverDataRecord, pResourcePrivateDriverData, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, pResourcePrivateDriverData);
AV_SAME_OFFSET(DxgkargGetStandardAllocationDriverDataRecord, ResourcePrivateDriverDataSize, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, ResourcePrivateDriverDataSize);
AV_SAME_OFFSET(DxgkargGetStandardAllocationDriverDataRecord, PhysicalAdapterIndex, DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA, PhysicalAdapterIndex);

AV_SAME_SIZE(DxgkOpenAllocationInfoRecord, DXGK_OPENALLOCATIONINFO);
AV_SAME_OFFSET(DxgkOpenAllocationInfoRecord, hAllocation, DXGK_OPENALLOCATIONINFO, hAllocation);
AV_SAME_OFFSET(DxgkOpenAllocationInfoRecord, pPrivateDriverData, DXGK_OPENALLOCATIONINFO, pPrivateDriverData);
AV_SAME_OFFSET(DxgkOpenAllocationInfoRecord, PrivateDriverDataSize, DXGK_OPENALLOCATIONINFO, PrivateDriverDataSize);
AV_SAME_OFFSET(DxgkOpenAllocationInfoRecord, hDeviceSpecificAllocation, DXGK_OPENALLOCATIONINFO, hDeviceSpecificAllocation);

AV_SAME_SIZE(DxgkargOpenAllocationRecord, DXGKARG_OPENALLOCATION);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, NumAllocations, DXGKARG_OPENALLOCATION, NumAllocations);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, pOpenAllocation, DXGKARG_OPENALLOCATION, pOpenAllocation);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, pPrivateDriverData, DXGKARG_OPENALLOCATION, pPrivateDriverData);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, PrivateDriverSize, DXGKARG_OPENALLOCATION, PrivateDriverSize);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, Flags, DXGKARG_OPENALLOCATION, Flags);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, SubresourceIndex, DXGKARG_OPENALLOCATION, SubresourceIndex);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, SubresourceOffset, DXGKARG_OPENALLOCATION, SubresourceOffset);
AV_SAME_OFFSET(DxgkargOpenAllocationRecord, Pitch, DXGKARG_OPENALLOCATION, Pitch);

AV_SAME_SIZE(DxgkargCloseAllocationRecord, DXGKARG_CLOSEALLOCATION);
AV_SAME_OFFSET(DxgkargCloseAllocationRecord, NumAllocations, DXGKARG_CLOSEALLOCATION, NumAllocations);
AV_SAME_OFFSET(DxgkargCloseAllocationRecord, pOpenHandleList, DXGKARG_CLOSEALLOCATION, pOpenHandleList);

AV_SAME_SIZE(DxgkargSetDisplayPrivateDriverFormatRecord, DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT);
AV_SAME_OFFSET(DxgkargSetDisplayPrivateDriverFormatRecord, VidPnSourceId, DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT, VidPnSourceId);
AV_SAME_OFFSET(DxgkargSetDisplayPrivateDriverFormatRecord, PrimaryAllocation, DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT, PrimaryAllocation);
AV_SAME_OFFSET(DxgkargSetDisplayPrivateDriverFormatRecord, PrivateDriverFormatAttribute, DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT, PrivateDriverFormatAttribute);

AV_SAME_SIZE(DxgkQuerySegmentOut4Record, DXGK_QUERYSEGMENTOUT4);
AV_SAME_OFFSET(DxgkQuerySegmentOut4Record, NbSegment, DXGK_QUERYSEGMENTOUT4, NbSegment);
AV_SAME_OFFSET(DxgkQuerySegmentOut4Record, pSegmentDescriptor, DXGK_QUERYSEGMENTOUT4, pSegmentDescriptor);
AV_SAME_OFFSET(DxgkQuerySegmentOut4Record, PagingBufferSegmentId, DXGK_QUERYSEGMENTOUT4, PagingBufferSegmentId);
AV_SAME_OFFSET(DxgkQuerySegmentOut4Record, PagingBufferSize, DXGK_QUERYSEGMENTOUT4, PagingBufferSize);
AV_SAME_OFFSET(DxgkQuerySegmentOut4Record, PagingBufferPrivateDataSize, DXGK_QUERYSEGMENTOUT4, PagingBufferPrivateDataSize);
AV_SAME_OFFSET(DxgkQuerySegmentOut4Record, SegmentDescriptorStride, DXGK_QUERYSEGMENTOUT4, SegmentDescriptorStride);

AV_SAME_SIZE(DxgkSegmentDescriptor4Record, DXGK_SEGMENTDESCRIPTOR4);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, Flags, DXGK_SEGMENTDESCRIPTOR4, Flags);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, BaseAddress, DXGK_SEGMENTDESCRIPTOR4, BaseAddress);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, Size, DXGK_SEGMENTDESCRIPTOR4, Size);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, CommitLimit, DXGK_SEGMENTDESCRIPTOR4, CommitLimit);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, SystemMemoryEndAddress, DXGK_SEGMENTDESCRIPTOR4, SystemMemoryEndAddress);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, CpuTranslatedAddress, DXGK_SEGMENTDESCRIPTOR4, CpuTranslatedAddress);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, CpuHostApertureSizeInPages, DXGK_SEGMENTDESCRIPTOR4, CpuHostAperture.SizeInPages);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, NumInvalidMemoryRanges, DXGK_SEGMENTDESCRIPTOR4, NumInvalidMemoryRanges);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, VprRangeStartOffset, DXGK_SEGMENTDESCRIPTOR4, VprRangeStartOffset);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, VprRangeSize, DXGK_SEGMENTDESCRIPTOR4, VprRangeSize);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, VprAlignment, DXGK_SEGMENTDESCRIPTOR4, VprAlignment);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, NumVprSupported, DXGK_SEGMENTDESCRIPTOR4, NumVprSupported);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, VprReserveSize, DXGK_SEGMENTDESCRIPTOR4, VprReserveSize);
AV_SAME_OFFSET(DxgkSegmentDescriptor4Record, NumUEFIFrameBufferRanges, DXGK_SEGMENTDESCRIPTOR4, NumUEFIFrameBufferRanges);

AV_SAME_SIZE(D3dkmdtSharedPrimarySurfaceDataRecord, D3DKMDT_SHAREDPRIMARYSURFACEDATA);
AV_SAME_OFFSET(D3dkmdtSharedPrimarySurfaceDataRecord, Width, D3DKMDT_SHAREDPRIMARYSURFACEDATA, Width);
AV_SAME_OFFSET(D3dkmdtSharedPrimarySurfaceDataRecord, Height, D3DKMDT_SHAREDPRIMARYSURFACEDATA, Height);
AV_SAME_OFFSET(D3dkmdtSharedPrimarySurfaceDataRecord, Format, D3DKMDT_SHAREDPRIMARYSURFACEDATA, Format);
AV_SAME_OFFSET(D3dkmdtSharedPrimarySurfaceDataRecord, RefreshRateNumerator, D3DKMDT_SHAREDPRIMARYSURFACEDATA, RefreshRate.Numerator);
AV_SAME_OFFSET(D3dkmdtSharedPrimarySurfaceDataRecord, RefreshRateDenominator, D3DKMDT_SHAREDPRIMARYSURFACEDATA, RefreshRate.Denominator);
AV_SAME_OFFSET(D3dkmdtSharedPrimarySurfaceDataRecord, VidPnSourceId, D3DKMDT_SHAREDPRIMARYSURFACEDATA, VidPnSourceId);

AV_SAME_SIZE(D3dkmdtShadowSurfaceDataRecord, D3DKMDT_SHADOWSURFACEDATA);
AV_SAME_OFFSET(D3dkmdtShadowSurfaceDataRecord, Width, D3DKMDT_SHADOWSURFACEDATA, Width);
AV_SAME_OFFSET(D3dkmdtShadowSurfaceDataRecord, Height, D3DKMDT_SHADOWSURFACEDATA, Height);
AV_SAME_OFFSET(D3dkmdtShadowSurfaceDataRecord, Format, D3DKMDT_SHADOWSURFACEDATA, Format);
AV_SAME_OFFSET(D3dkmdtShadowSurfaceDataRecord, Pitch, D3DKMDT_SHADOWSURFACEDATA, Pitch);

AV_SAME_SIZE(D3dkmdtStagingSurfaceDataRecord, D3DKMDT_STAGINGSURFACEDATA);
AV_SAME_OFFSET(D3dkmdtStagingSurfaceDataRecord, Width, D3DKMDT_STAGINGSURFACEDATA, Width);
AV_SAME_OFFSET(D3dkmdtStagingSurfaceDataRecord, Height, D3DKMDT_STAGINGSURFACEDATA, Height);
AV_SAME_OFFSET(D3dkmdtStagingSurfaceDataRecord, Pitch, D3DKMDT_STAGINGSURFACEDATA, Pitch);

AV_SAME_SIZE(D3dkmdtGdiSurfaceDataRecord, D3DKMDT_GDISURFACEDATA);
AV_SAME_OFFSET(D3dkmdtGdiSurfaceDataRecord, Width, D3DKMDT_GDISURFACEDATA, Width);
AV_SAME_OFFSET(D3dkmdtGdiSurfaceDataRecord, Height, D3DKMDT_GDISURFACEDATA, Height);
AV_SAME_OFFSET(D3dkmdtGdiSurfaceDataRecord, Format, D3DKMDT_GDISURFACEDATA, Format);
AV_SAME_OFFSET(D3dkmdtGdiSurfaceDataRecord, Type, D3DKMDT_GDISURFACEDATA, Type);
AV_SAME_OFFSET(D3dkmdtGdiSurfaceDataRecord, Flags, D3DKMDT_GDISURFACEDATA, Flags);
AV_SAME_OFFSET(D3dkmdtGdiSurfaceDataRecord, Pitch, D3DKMDT_GDISURFACEDATA, Pitch);

static_assert(D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE == 1 && D3DKMDT_STANDARDALLOCATION_GDISURFACE == 4 &&
                  D3DKMDT_GDISURFACE_TEXTURE_CPUVISIBLE == 6 && DXGKQAITYPE_QUERYSEGMENT4 == 11 &&
                  DXGK_HANDLE_ALLOCATION == 1 && D3DDDIFMT_A8R8G8B8 == 21 && D3DDDIFMT_X8B8G8R8 == 33,
              "standard allocation, GDI surface, query and format values");

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
