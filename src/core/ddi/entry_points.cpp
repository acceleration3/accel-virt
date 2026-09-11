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

int32_t AvDdiQueryDeviceDescriptor(void *context, uint32_t child, av::DxgkDeviceDescriptorRecord *descriptor)
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

int32_t AvDdiRender(void *context, av::DxgkargRenderRecord *request)
{ return static_cast<av::Context *>(context)->device().adapter().render(context, request); }

int32_t AvDdiPatch(void *context, const av::DxgkargPatchRecord *request)
{ return static_cast<av::Adapter *>(context)->patch(request); }

int32_t AvDdiSubmitCommand(void *context, const av::DxgkargSubmitCommandRecord *request)
{ return static_cast<av::Adapter *>(context)->submit_command(request); }

int32_t AvDdiPreemptCommand(void *context, const av::DxgkargPreemptCommandRecord *request)
{ return static_cast<av::Adapter *>(context)->preempt_command(request); }

int32_t AvDdiQueryCurrentFence(void *context, av::DxgkargQueryCurrentFenceRecord *request)
{ return static_cast<av::Adapter *>(context)->query_current_fence(request); }

int32_t AvDdiBuildPagingBuffer(void *context, av::DxgkargBuildPagingBufferRecord *request)
{ return static_cast<av::Adapter *>(context)->build_paging_buffer(request); }

int32_t AvDdiResetFromTimeout(void *context)
{ return static_cast<av::Adapter *>(context)->reset_from_timeout(); }

int32_t AvDdiRestartFromTimeout(void *context)
{ return static_cast<av::Adapter *>(context)->restart_from_timeout(); }

int32_t AvDdiIsSupportedVidPn(void *context, av::DxgkargIsSupportedVidPnRecord *request)
{ return static_cast<av::Adapter *>(context)->is_supported_vidpn(request); }

int32_t AvDdiRecommendFunctionalVidPn(void *context, const av::DxgkargRecommendFunctionalVidPnRecord *request)
{ return static_cast<av::Adapter *>(context)->recommend_functional_vidpn(request); }

int32_t AvDdiEnumVidPnCofuncModality(void *context, const av::DxgkargEnumVidPnCofuncModalityRecord *request)
{ return static_cast<av::Adapter *>(context)->enum_vidpn_cofunc_modality(request); }

int32_t AvDdiCommitVidPn(void *context, const av::DxgkargCommitVidPnRecord *request)
{ return static_cast<av::Adapter *>(context)->commit_vidpn(request); }

int32_t AvDdiUpdateActiveVidPnPresentPath(void *context, const av::D3dkmdtVidPnPresentPathRecord *path)
{ return static_cast<av::Adapter *>(context)->update_active_vidpn_present_path(path); }

int32_t AvDdiRecommendMonitorModes(void *context, const av::DxgkargRecommendMonitorModesRecord *request)
{ return static_cast<av::Adapter *>(context)->recommend_monitor_modes(request); }

int32_t AvDdiRecommendVidPnTopology(void *context, const av::DxgkargRecommendVidPnTopologyRecord *request)
{ return static_cast<av::Adapter *>(context)->recommend_vidpn_topology(request); }

int32_t AvDdiGetScanLine(void *context, av::DxgkargGetScanLineRecord *request)
{ return static_cast<av::Adapter *>(context)->get_scan_line(request); }

int32_t AvDdiControlInterrupt(void *context, int32_t type, uint8_t enable)
{ return static_cast<av::Adapter *>(context)->control_interrupt(type, enable); }

int32_t AvDdiSetVidPnSourceAddress(void *context, const av::DxgkargSetVidPnSourceAddressRecord *request)
{ return static_cast<av::Adapter *>(context)->set_vidpn_source_address(request); }

int32_t AvDdiSetVidPnSourceVisibility(void *context, const av::DxgkargSetVidPnSourceVisibilityRecord *request)
{ return static_cast<av::Adapter *>(context)->set_vidpn_source_visibility(request); }

int32_t AvDdiPresent(void *context, av::DxgkargPresentRecord *request)
{ return static_cast<av::Context *>(context)->device().adapter().present(context, request); }

int32_t AvDdiSetPointerShape(void *context, const av::DxgkargSetPointerShapeRecord *request)
{ return static_cast<av::Adapter *>(context)->set_pointer_shape(request); }

int32_t AvDdiSetPointerPosition(void *context, const av::DxgkargSetPointerPositionRecord *request)
{ return static_cast<av::Adapter *>(context)->set_pointer_position(request); }

int32_t AvDdiAcquireSwizzlingRange(void *context, void *request)
{ return static_cast<av::Adapter *>(context)->acquire_swizzling_range(request); }

int32_t AvDdiReleaseSwizzlingRange(void *context, const void *request)
{ return static_cast<av::Adapter *>(context)->release_swizzling_range(request); }

int32_t AvDdiSetPalette(void *context, const void *request)
{ return static_cast<av::Adapter *>(context)->set_palette(request); }

int32_t AvDdiStopCapture(void *context, const void *request)
{ return static_cast<av::Adapter *>(context)->stop_capture(request); }

int32_t AvDdiCreateOverlay(void *context, void *request)
{ return static_cast<av::Adapter *>(context)->create_overlay(request); }

int32_t AvDdiQueryVidPnHWCapability(void *context, av::DxgkargQueryVidPnHwCapabilityRecord *request)
{ return static_cast<av::Adapter *>(context)->query_vidpn_hw_capability(request); }

int32_t AvDdiSetPowerComponentFState(void *context, uint32_t component, uint32_t fstate)
{ return static_cast<av::Adapter *>(context)->set_power_component_fstate(component, fstate); }

int32_t AvDdiQueryDependentEngineGroup(void *context, av::DxgkargQueryDependentEngineGroupRecord *request)
{ return static_cast<av::Adapter *>(context)->query_dependent_engine_group(request); }

int32_t AvDdiQueryEngineStatus(void *context, av::DxgkargQueryEngineStatusRecord *request)
{ return static_cast<av::Adapter *>(context)->query_engine_status(request); }

int32_t AvDdiResetEngine(void *context, av::DxgkargResetEngineRecord *request)
{ return static_cast<av::Adapter *>(context)->reset_engine(request); }

int32_t AvDdiStopDeviceAndReleasePostDisplayOwnership(void *context, uint32_t target,
                                                      av::DxgkDisplayInformationRecord *display)
{ return static_cast<av::Adapter *>(context)->stop_device_and_release_post_display_ownership(target, display); }

int32_t AvDdiSystemDisplayEnable(void *context, uint32_t target, void *flags, uint32_t *width, uint32_t *height,
                                 int32_t *format)
{ return static_cast<av::Adapter *>(context)->system_display_enable(target, flags, width, height, format); }

void AvDdiSystemDisplayWrite(void *context, const void *source, uint32_t width, uint32_t height, uint32_t stride,
                             uint32_t x, uint32_t y)
{ static_cast<av::Adapter *>(context)->system_display_write(source, width, height, stride, x, y); }

int32_t AvDdiCancelCommand(void *context, const av::DxgkargCancelCommandRecord *request)
{ return static_cast<av::Adapter *>(context)->cancel_command(request); }

int32_t AvDdiGetChildContainerId(void *context, uint32_t child, av::DxgkChildContainerIdRecord *container)
{ return static_cast<av::Adapter *>(context)->get_child_container_id(child, container); }

int32_t AvDdiPowerRuntimeControlRequest(void *context, const void *control_code, const void *input, size_t input_size,
                                        void *output, size_t output_size, size_t *returned)
{
    return static_cast<av::Adapter *>(context)->power_runtime_control_request(control_code, input, input_size, output,
                                                                             output_size, returned);
}

int32_t AvDdiSetVidPnSourceAddressWithMultiPlaneOverlay(void *context, const void *request)
{ return static_cast<av::Adapter *>(context)->set_vidpn_source_address_with_multi_plane_overlay(request); }

int32_t AvDdiNotifySurpriseRemoval(void *context, int32_t removal_type)
{ return static_cast<av::Adapter *>(context)->notify_surprise_removal(removal_type); }

int32_t AvDdiUpdateOverlay(void *, const void *)
{ return av::NtStatus(av::Status::not_supported).value(); }

int32_t AvDdiFlipOverlay(void *, const void *)
{ return av::NtStatus(av::Status::not_supported).value(); }

int32_t AvDdiDestroyOverlay(void *)
{ return av::NtStatus(av::Status::not_supported).value(); }

uint8_t AvSynchronizedNotify(void *notification)
{ return static_cast<av::InterruptNotification *>(notification)->notify(); }

void AvVsyncTick(void *, void *vsync)
{ static_cast<av::SyntheticVsync *>(vsync)->tick(); }

}
