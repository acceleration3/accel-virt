/* DXGKRNL_INTERFACE at DXGKDDI_INTERFACE_VERSION_WDDM3_2: the callback table
 * dxgkrnl passes to DxgkDdiStartDevice. A DDI record: fields keep the WDK's
 * names and src/km/ddi_record_layout.cpp proves each offset and the size.
 * Callbacks core calls are typed; the rest are placeholders until a feature
 * calls them. Signatures follow dispmprt.h: HANDLE and PVOID are void *,
 * PHYSICAL_ADDRESS int64_t, ULONG uint32_t, BOOLEAN uint8_t, enums and
 * NTSTATUS int32_t. */
#ifndef AV_DXGKRNL_INTERFACE_RECORD_HPP
#define AV_DXGKRNL_INTERFACE_RECORD_HPP

#include "core/ddi/dxgk_device_info_record.hpp"
#include "core/ddi/dxgk_vidpn_interface_records.hpp"
#include "core/ddi/dxgkargcb_get_handle_data_record.hpp"
#include "core/ddi/dxgkargcb_release_handle_data_record.hpp"
#include "core/ddi/dxgkargcb_notify_interrupt_data_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargcbQueryFeatureSupportRecord;
using DxgkCbQueryFeatureSupport = int32_t (*)(DxgkargcbQueryFeatureSupportRecord *arguments);

using DxgkCbGetDeviceInformation = int32_t (*)(void *device_handle, DxgkDeviceInfoRecord *device_info);
using DxgkCbMapMemory = int32_t (*)(void *device_handle, int64_t translated_address, uint32_t length,
                                    uint8_t in_io_space, uint8_t map_to_user_mode, int32_t cache_type,
                                    void **virtual_address);
using DxgkCbQueueDpc = uint8_t (*)(void *device_handle);
using DxgkCbReadDeviceSpace = int32_t (*)(void *device_handle, uint32_t data_type, void *buffer,
                                          uint32_t offset, uint32_t length, uint32_t *bytes_read);
using DxgkCbUnmapMemory = int32_t (*)(void *device_handle, void *virtual_address);
/* No device handle: dxgkrnl resolves the handle in the calling thread's DDI. */
using DxgkCbGetHandleData = void *(*)(const DxgkargcbGetHandleDataRecord *arguments);
/* WDDM 2.0's replacement pair for DxgkCbGetHandleData, which answers null for an
 * allocation being opened: the data is borrowed until the release handle the
 * acquire answers is handed back. basicrender.sys's WarpKMOpenAllocation
 * (@1400021c0) acquires, reads, and releases inside the one DDI. */
using DxgkCbAcquireHandleData = void *(*)(const DxgkargcbGetHandleDataRecord *arguments, void **release_handle);
using DxgkCbReleaseHandleData = void (*)(DxgkargcbReleaseHandleDataRecord arguments);
/* PKSYNCHRONIZE_ROUTINE: runs at the adapter's device IRQL. */
using SynchronizeRoutine = uint8_t (*)(void *context);
using DxgkCbSynchronizeExecution = int32_t (*)(void *device_handle, SynchronizeRoutine routine, void *context,
                                               uint32_t message_number, uint8_t *return_value);
using DxgkCbNotifyInterrupt = void (*)(void *device_handle, const DxgkargcbNotifyInterruptDataRecord *data);
using DxgkCbNotifyDpc = void (*)(void *device_handle);
/* No device handle: the VidPn handle names the adapter. */
using DxgkCbQueryVidPnInterface = int32_t (*)(void *vidpn, int32_t version, const DxgkVidPnInterfaceRecord **vidpn_interface);

struct DxgkrnlInterfaceRecord {
    uint32_t                   Size;
    uint32_t                   Version;
    void                      *DeviceHandle;
    void                      *DxgkCbEvalAcpiMethod;
    DxgkCbGetDeviceInformation DxgkCbGetDeviceInformation;
    void                      *DxgkCbIndicateChildStatus;
    DxgkCbMapMemory            DxgkCbMapMemory;
    DxgkCbQueueDpc             DxgkCbQueueDpc;
    void                      *DxgkCbQueryServices;
    DxgkCbReadDeviceSpace      DxgkCbReadDeviceSpace;
    DxgkCbSynchronizeExecution DxgkCbSynchronizeExecution;
    DxgkCbUnmapMemory          DxgkCbUnmapMemory;
    void                      *DxgkCbWriteDeviceSpace;
    void                      *DxgkCbIsDevicePresent;
    DxgkCbGetHandleData        DxgkCbGetHandleData;
    void                      *DxgkCbGetHandleParent;
    void                      *DxgkCbEnumHandleChildren;
    DxgkCbNotifyInterrupt      DxgkCbNotifyInterrupt;
    DxgkCbNotifyDpc            DxgkCbNotifyDpc;
    DxgkCbQueryVidPnInterface  DxgkCbQueryVidPnInterface;
    void                      *DxgkCbQueryMonitorInterface;
    void                      *DxgkCbGetCaptureAddress;
    void                      *DxgkCbLogEtwEvent;
    void                      *DxgkCbExcludeAdapterAccess;
    /* DxgkCbCreateContextAllocation through DxgkCbReserveGpuVirtualAddressRange. */
    void                      *LaterCallbacks[13];
    DxgkCbAcquireHandleData    DxgkCbAcquireHandleData;
    DxgkCbReleaseHandleData    DxgkCbReleaseHandleData;
    /* DxgkCbHardwareContentProtectionTeardown through DxgkCbNotifyCursorSupportChange. */
    void                      *CallbacksBeforeFeatureSupport[23];
    /* What the OS answers about a feature the driver implements, which is what
     * a driver must ask before it reports a scheduling capability (feature
     * 049, research.md §15). */
    DxgkCbQueryFeatureSupport  DxgkCbQueryFeatureSupport;
    /* DxgkCbCreatePhysicalMemoryObject through DxgkCbDisconnectDoorbell. */
    void                      *CallbacksAfterFeatureSupport[10];
};

}  // namespace av

#endif  // AV_DXGKRNL_INTERFACE_RECORD_HPP
