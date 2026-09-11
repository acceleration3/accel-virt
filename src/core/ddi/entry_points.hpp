/* The DDI entry points registered in the miniport's DdiTable. Constitution III's
 * ABI exception: extern "C", one statement each, forwarding to the adapter
 * behind the context or, for the context-less DDIs, to the bound root.
 * Signatures mirror dispmprt.h at DXGKDDI_INTERFACE_VERSION_WDDM3_2: ULONG is
 * uint32_t, BOOLEAN and UCHAR uint8_t, NTSTATUS and enums int32_t. */
#ifndef AV_DDI_ENTRY_POINTS_HPP
#define AV_DDI_ENTRY_POINTS_HPP

#include "../context.hpp"
#include "../device.hpp"
#include "../miniport_binding.hpp"
#include "dxgk_child_descriptor_record.hpp"
#include "dxgk_child_status_record.hpp"
#include "dxgkarg_close_allocation_record.hpp"
#include "dxgkarg_collect_dbg_info_record.hpp"
#include "dxgkarg_create_allocation_record.hpp"
#include "dxgkarg_create_context_record.hpp"
#include "dxgkarg_create_device_record.hpp"
#include "dxgkarg_describe_allocation_record.hpp"
#include "dxgkarg_destroy_allocation_record.hpp"
#include "dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "dxgkarg_open_allocation_record.hpp"
#include "dxgkarg_set_display_private_driver_format_record.hpp"
#include "dxgkarg_escape_record.hpp"
#include "dxgkarg_query_adapter_info_record.hpp"
#include "dxgkrnl_interface_record.hpp"
#include "linked_device_record.hpp"

#include <stdint.h>

namespace av {

extern MiniportBinding miniport_binding;

}  // namespace av

extern "C" {

int32_t AvDdiAddDevice(void *physical_device_object, void **context);
int32_t AvDdiStartDevice(void *context, void *start_info, const av::DxgkrnlInterfaceRecord *dxgkrnl_interface,
                         uint32_t *sources, uint32_t *children);
int32_t AvDdiStopDevice(void *context);
int32_t AvDdiRemoveDevice(void *context);
int32_t AvDdiDispatchIoRequest(void *context, uint32_t source, void *request);
uint8_t AvDdiInterruptRoutine(void *context, uint32_t message);
void    AvDdiDpcRoutine(void *context);
int32_t AvDdiQueryChildRelations(void *context, av::DxgkChildDescriptorRecord *relations, uint32_t size);
int32_t AvDdiQueryChildStatus(void *context, av::DxgkChildStatusRecord *status, uint8_t non_destructive);
int32_t AvDdiQueryDeviceDescriptor(void *context, uint32_t child, void *descriptor);
int32_t AvDdiSetPowerState(void *context, uint32_t device, int32_t power_state, int32_t action);
int32_t AvDdiNotifyAcpiEvent(void *context, int32_t type, uint32_t event, void *argument,
                             uint32_t *acpi_flags);
void    AvDdiResetDevice(void *context);
void    AvDdiUnload(void);
int32_t AvDdiQueryInterface(void *context, void *query);
void    AvDdiControlEtwLogging(uint8_t enable, uint32_t flags, uint8_t level);
int32_t AvDdiLinkDevice(void *physical_device_object, void *context, av::LinkedDeviceRecord *linked);
int32_t AvDdiQueryAdapterInfo(void *context, const av::DxgkargQueryAdapterInfoRecord *query);
int32_t AvDdiEscape(void *context, const av::DxgkargEscapeRecord *escape);
int32_t AvDdiCollectDbgInfo(void *context, const av::DxgkargCollectDbgInfoRecord *collect);
int32_t AvDdiCreateDevice(void *context, av::DxgkargCreateDeviceRecord *request);
int32_t AvDdiDestroyDevice(void *device);
int32_t AvDdiCreateAllocation(void *context, av::DxgkargCreateAllocationRecord *request);
int32_t AvDdiDestroyAllocation(void *context, const av::DxgkargDestroyAllocationRecord *request);
int32_t AvDdiDescribeAllocation(void *context, av::DxgkargDescribeAllocationRecord *request);
int32_t AvDdiGetStandardAllocationDriverData(void *context, av::DxgkargGetStandardAllocationDriverDataRecord *request);
/* The WDK passes the argument block as const; the driver answers in it. */
int32_t AvDdiOpenAllocation(void *device, av::DxgkargOpenAllocationRecord *request);
int32_t AvDdiCloseAllocation(void *device, const av::DxgkargCloseAllocationRecord *request);
int32_t AvDdiSetDisplayPrivateDriverFormat(void *context, const av::DxgkargSetDisplayPrivateDriverFormatRecord *request);
int32_t AvDdiCreateContext(void *device, av::DxgkargCreateContextRecord *request);
int32_t AvDdiDestroyContext(void *context);

}

#endif  // AV_DDI_ENTRY_POINTS_HPP
