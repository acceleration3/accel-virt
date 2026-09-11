/* Proves at compile time that each DDI record in src/core/ddi/ is laid out
 * byte-for-byte as the WDK structure dxgkrnl passes, so core can be driven on
 * Linux through the same memory the kernel hands it. */
#include <ntddk.h>
#include <dispmprt.h>

#include <stddef.h>

#include "ddi/dxgk_child_descriptor_record.hpp"
#include "ddi/dxgk_child_status_record.hpp"
#include "ddi/dxgk_device_info_record.hpp"
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
