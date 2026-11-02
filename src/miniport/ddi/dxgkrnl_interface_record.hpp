#ifndef AV_DXGKRNL_INTERFACE_RECORD_HPP
#define AV_DXGKRNL_INTERFACE_RECORD_HPP

#include "miniport/ddi/dxgk_device_info_record.hpp"
#include "miniport/ddi/dxgk_vidpn_interface_records.hpp"
#include "miniport/ddi/dxgkargcb_get_handle_data_record.hpp"
#include "miniport/ddi/dxgkargcb_release_handle_data_record.hpp"
#include "miniport/ddi/dxgkargcb_notify_interrupt_data_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkChildStatusRecord;
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
using DxgkCbGetHandleData = void *(*)(const DxgkargcbGetHandleDataRecord *arguments);
using DxgkCbAcquireHandleData = void *(*)(const DxgkargcbGetHandleDataRecord *arguments, void **release_handle);
using DxgkCbReleaseHandleData = void (*)(DxgkargcbReleaseHandleDataRecord arguments);
using SynchronizeRoutine = uint8_t (*)(void *context);
using DxgkCbSynchronizeExecution = int32_t (*)(void *device_handle, SynchronizeRoutine routine, void *context,
                                               uint32_t message_number, uint8_t *return_value);
using DxgkCbNotifyInterrupt = void (*)(void *device_handle, const DxgkargcbNotifyInterruptDataRecord *data);
using DxgkCbNotifyDpc = void (*)(void *device_handle);
using DxgkCbIndicateChildStatus = int32_t (*)(void *device_handle, const DxgkChildStatusRecord *child_status);
using DxgkCbQueryVidPnInterface = int32_t (*)(void *vidpn, int32_t version, const DxgkVidPnInterfaceRecord **vidpn_interface);

struct DxgkrnlInterfaceRecord {
    uint32_t                   Size;
    uint32_t                   Version;
    void                      *DeviceHandle;
    void                      *DxgkCbEvalAcpiMethod;
    DxgkCbGetDeviceInformation DxgkCbGetDeviceInformation;
    DxgkCbIndicateChildStatus  DxgkCbIndicateChildStatus;
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
    void                      *LaterCallbacks[13];
    DxgkCbAcquireHandleData    DxgkCbAcquireHandleData;
    DxgkCbReleaseHandleData    DxgkCbReleaseHandleData;
    void                      *CallbacksBeforeFeatureSupport[23];
    DxgkCbQueryFeatureSupport  DxgkCbQueryFeatureSupport;
    void                      *CallbacksAfterFeatureSupport[10];
};

}

#endif
