/* DXGKRNL_INTERFACE at DXGKDDI_INTERFACE_VERSION_WDDM3_2: the callback table
 * dxgkrnl passes to DxgkDdiStartDevice. A DDI record: fields keep the WDK's
 * names and src/km/ddi_record_layout.cpp proves each offset and the size.
 * Callbacks core calls are typed; the rest are placeholders until a feature
 * calls them. Signatures follow dispmprt.h: HANDLE and PVOID are void *,
 * PHYSICAL_ADDRESS int64_t, ULONG uint32_t, BOOLEAN uint8_t, enums and
 * NTSTATUS int32_t. */
#ifndef AV_DXGKRNL_INTERFACE_RECORD_HPP
#define AV_DXGKRNL_INTERFACE_RECORD_HPP

#include "dxgk_device_info_record.hpp"

#include <stdint.h>

namespace av {

using DxgkCbGetDeviceInformation = int32_t (*)(void *device_handle, DxgkDeviceInfoRecord *device_info);
using DxgkCbMapMemory = int32_t (*)(void *device_handle, int64_t translated_address, uint32_t length,
                                    uint8_t in_io_space, uint8_t map_to_user_mode, int32_t cache_type,
                                    void **virtual_address);
using DxgkCbQueueDpc = uint8_t (*)(void *device_handle);
using DxgkCbReadDeviceSpace = int32_t (*)(void *device_handle, uint32_t data_type, void *buffer,
                                          uint32_t offset, uint32_t length, uint32_t *bytes_read);
using DxgkCbUnmapMemory = int32_t (*)(void *device_handle, void *virtual_address);

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
    void                      *DxgkCbSynchronizeExecution;
    DxgkCbUnmapMemory          DxgkCbUnmapMemory;
    void                      *DxgkCbWriteDeviceSpace;
    void                      *DxgkCbIsDevicePresent;
    void                      *DxgkCbGetHandleData;
    void                      *DxgkCbGetHandleParent;
    void                      *DxgkCbEnumHandleChildren;
    void                      *DxgkCbNotifyInterrupt;
    void                      *DxgkCbNotifyDpc;
    void                      *DxgkCbQueryVidPnInterface;
    void                      *DxgkCbQueryMonitorInterface;
    void                      *DxgkCbGetCaptureAddress;
    void                      *DxgkCbLogEtwEvent;
    void                      *DxgkCbExcludeAdapterAccess;
    /* DxgkCbCreateContextAllocation through DxgkCbDisconnectDoorbell. */
    void                      *LaterCallbacks[49];
};

}  // namespace av

#endif  // AV_DXGKRNL_INTERFACE_RECORD_HPP
