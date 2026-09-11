#include "entry_points.hpp"

#include "../adapter.hpp"

namespace av {

constinit MiniportBinding miniport_binding;

}  // namespace av

extern "C" {

int32_t AvDdiAddDevice(void *physical_device_object, void **context)
{ return av::miniport_binding.add_device(physical_device_object, context); }

int32_t AvDdiStartDevice(void *context, void *start_info, const av::DxgkrnlInterfaceRecord *dxgkrnl_interface,
                         uint32_t *sources, uint32_t *children)
{ return static_cast<av::Adapter *>(context)->start_device(start_info, dxgkrnl_interface, sources, children); }

int32_t AvDdiStopDevice(void *context)
{ return static_cast<av::Adapter *>(context)->stop_device(); }

int32_t AvDdiRemoveDevice(void *context)
{ return av::miniport_binding.remove_device(context); }

int32_t AvDdiDispatchIoRequest(void *context, uint32_t source, void *request)
{ return static_cast<av::Adapter *>(context)->dispatch_io_request(source, request); }

uint8_t AvDdiInterruptRoutine(void *context, uint32_t message)
{ return static_cast<av::Adapter *>(context)->interrupt_routine(message); }

void AvDdiDpcRoutine(void *context)
{ static_cast<av::Adapter *>(context)->dpc_routine(); }

int32_t AvDdiQueryChildRelations(void *context, av::DxgkChildDescriptorRecord *relations, uint32_t size)
{ return static_cast<av::Adapter *>(context)->query_child_relations(relations, size); }

int32_t AvDdiQueryChildStatus(void *context, av::DxgkChildStatusRecord *status, uint8_t non_destructive)
{ return static_cast<av::Adapter *>(context)->query_child_status(status, non_destructive); }

int32_t AvDdiQueryDeviceDescriptor(void *context, uint32_t child, void *descriptor)
{ return static_cast<av::Adapter *>(context)->query_device_descriptor(child, descriptor); }

int32_t AvDdiSetPowerState(void *context, uint32_t device, int32_t power_state, int32_t action)
{ return static_cast<av::Adapter *>(context)->set_power_state(device, power_state, action); }

int32_t AvDdiNotifyAcpiEvent(void *context, int32_t type, uint32_t event, void *argument,
                             uint32_t *acpi_flags)
{ return static_cast<av::Adapter *>(context)->notify_acpi_event(type, event, argument, acpi_flags); }

void AvDdiResetDevice(void *context)
{ static_cast<av::Adapter *>(context)->reset_device(); }

void AvDdiUnload(void)
{ av::miniport_binding.unload(); }

int32_t AvDdiQueryInterface(void *context, void *query)
{ return static_cast<av::Adapter *>(context)->query_interface(query); }

/* The driver emits no ETW events; enabling them changes nothing. */
void AvDdiControlEtwLogging(uint8_t, uint32_t, uint8_t)
{}

int32_t AvDdiLinkDevice(void *physical_device_object, void *context, av::LinkedDeviceRecord *linked)
{ return static_cast<av::Adapter *>(context)->link_device(physical_device_object, linked); }

int32_t AvDdiQueryAdapterInfo(void *context, const av::DxgkargQueryAdapterInfoRecord *query)
{ return static_cast<av::Adapter *>(context)->query_adapter_info(query); }

int32_t AvDdiEscape(void *context, const av::DxgkargEscapeRecord *escape)
{ return static_cast<av::Adapter *>(context)->escape(escape); }

int32_t AvDdiCollectDbgInfo(void *context, const av::DxgkargCollectDbgInfoRecord *collect)
{ return static_cast<av::Adapter *>(context)->collect_dbg_info(collect); }

int32_t AvDdiCreateDevice(void *context, av::DxgkargCreateDeviceRecord *request)
{ return static_cast<av::Adapter *>(context)->create_device(request); }

int32_t AvDdiDestroyDevice(void *device)
{ return static_cast<av::Device *>(device)->adapter().destroy_device(device); }

int32_t AvDdiCreateAllocation(void *context, av::DxgkargCreateAllocationRecord *request)
{ return static_cast<av::Adapter *>(context)->create_allocation(request); }

int32_t AvDdiDestroyAllocation(void *context, const av::DxgkargDestroyAllocationRecord *request)
{ return static_cast<av::Adapter *>(context)->destroy_allocation(request); }

int32_t AvDdiDescribeAllocation(void *context, av::DxgkargDescribeAllocationRecord *request)
{ return static_cast<av::Adapter *>(context)->describe_allocation(request); }

int32_t AvDdiGetStandardAllocationDriverData(void *context, av::DxgkargGetStandardAllocationDriverDataRecord *request)
{ return static_cast<av::Adapter *>(context)->get_standard_allocation_driver_data(request); }

int32_t AvDdiOpenAllocation(void *device, av::DxgkargOpenAllocationRecord *request)
{ return static_cast<av::Device *>(device)->adapter().open_allocation(device, request); }

int32_t AvDdiCloseAllocation(void *device, const av::DxgkargCloseAllocationRecord *request)
{ return static_cast<av::Device *>(device)->adapter().close_allocation(device, request); }

int32_t AvDdiSetDisplayPrivateDriverFormat(void *context, const av::DxgkargSetDisplayPrivateDriverFormatRecord *request)
{ return static_cast<av::Adapter *>(context)->set_display_private_driver_format(request); }

int32_t AvDdiCreateContext(void *device, av::DxgkargCreateContextRecord *request)
{ return static_cast<av::Device *>(device)->adapter().create_context(device, request); }

int32_t AvDdiDestroyContext(void *context)
{ return static_cast<av::Context *>(context)->device().adapter().destroy_context(context); }

}
