/* The DDI entry points registered in the miniport's DdiTable. Constitution III's
 * ABI exception: extern "C", one statement each, forwarding to the adapter
 * behind the context or, for the context-less DDIs, to the bound root.
 * Signatures mirror dispmprt.h at DXGKDDI_INTERFACE_VERSION_WDDM3_2: ULONG is
 * uint32_t, BOOLEAN and UCHAR uint8_t, NTSTATUS and enums int32_t. */
#ifndef AV_DDI_ENTRY_POINTS_HPP
#define AV_DDI_ENTRY_POINTS_HPP

#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/ddi/dxgk_child_container_id_record.hpp"
#include "core/ddi/dxgk_child_descriptor_record.hpp"
#include "core/ddi/dxgk_child_status_record.hpp"
#include "core/ddi/dxgk_device_descriptor_record.hpp"
#include "core/ddi/dxgk_display_information_record.hpp"
#include "core/ddi/dxgk_gpu_clock_data_record.hpp"
#include "core/ddi/dxgk_node_metadata_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/ddi/dxgkarg_cancel_command_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "core/ddi/dxgkarg_control_interrupt2_record.hpp"
#include "core/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_create_process_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_doorbell_records.hpp"
#include "core/ddi/dxgkarg_native_fence_records.hpp"
#include "core/ddi/dxgkddi_feature_interface_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "core/ddi/dxgkarg_escape_record.hpp"
#include "core/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "core/ddi/dxgkarg_get_scan_line_record.hpp"
#include "core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/ddi/dxgkarg_patch_record.hpp"
#include "core/ddi/dxgkarg_preempt_command_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "core/ddi/dxgkarg_query_dependent_engine_group_record.hpp"
#include "core/ddi/dxgkarg_query_engine_status_record.hpp"
#include "core/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
#include "core/ddi/dxgkarg_recommend_functional_vidpn_record.hpp"
#include "core/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "core/ddi/dxgkarg_recommend_vidpn_topology_record.hpp"
#include "core/ddi/dxgkarg_render_record.hpp"
#include "core/ddi/dxgkarg_reset_engine_record.hpp"
#include "core/ddi/dxgkarg_reset_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_resume_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/ddi/dxgkarg_set_stable_power_state_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_address_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_address_with_multi_plane_overlay_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_visibility_record.hpp"
#include "core/ddi/dxgkarg_submit_command_record.hpp"
#include "core/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "core/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "core/ddi/dxgkrnl_interface_record.hpp"
#include "core/ddi/linked_device_record.hpp"
#include "core/display/synthetic_vsync.hpp"
#include "core/miniport/interrupt_notification.hpp"
#include "core/miniport/miniport_binding.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/device.hpp"

#include <stddef.h>
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
int32_t AvDdiQueryDeviceDescriptor(void *context, uint32_t child, av::DxgkDeviceDescriptorRecord *descriptor);
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
int32_t AvDdiRender(void *context, av::DxgkargRenderRecord *request);
int32_t AvDdiPatch(void *context, const av::DxgkargPatchRecord *request);
int32_t AvDdiSubmitCommand(void *context, const av::DxgkargSubmitCommandRecord *request);
int32_t AvDdiPreemptCommand(void *context, const av::DxgkargPreemptCommandRecord *request);
int32_t AvDdiQueryCurrentFence(void *context, av::DxgkargQueryCurrentFenceRecord *request);
int32_t AvDdiBuildPagingBuffer(void *context, av::DxgkargBuildPagingBufferRecord *request);
int32_t AvDdiResetFromTimeout(void *context);
int32_t AvDdiRestartFromTimeout(void *context);
int32_t AvDdiIsSupportedVidPn(void *context, av::DxgkargIsSupportedVidPnRecord *request);
int32_t AvDdiRecommendFunctionalVidPn(void *context, const av::DxgkargRecommendFunctionalVidPnRecord *request);
int32_t AvDdiEnumVidPnCofuncModality(void *context, const av::DxgkargEnumVidPnCofuncModalityRecord *request);
int32_t AvDdiCommitVidPn(void *context, const av::DxgkargCommitVidPnRecord *request);
int32_t AvDdiUpdateActiveVidPnPresentPath(void *context, const av::D3dkmdtVidPnPresentPathRecord *path);
int32_t AvDdiRecommendMonitorModes(void *context, const av::DxgkargRecommendMonitorModesRecord *request);
int32_t AvDdiRecommendVidPnTopology(void *context, const av::DxgkargRecommendVidPnTopologyRecord *request);
int32_t AvDdiGetScanLine(void *context, av::DxgkargGetScanLineRecord *request);
int32_t AvDdiControlInterrupt(void *context, int32_t type, uint8_t enable);
int32_t AvDdiSetVidPnSourceAddress(void *context, const av::DxgkargSetVidPnSourceAddressRecord *request);
int32_t AvDdiSetVidPnSourceVisibility(void *context, const av::DxgkargSetVidPnSourceVisibilityRecord *request);
int32_t AvDdiPresent(void *context, av::DxgkargPresentRecord *request);
int32_t AvDdiSetPointerShape(void *context, const av::DxgkargSetPointerShapeRecord *request);
int32_t AvDdiSetPointerPosition(void *context, const av::DxgkargSetPointerPositionRecord *request);
int32_t AvDdiAcquireSwizzlingRange(void *context, void *request);
int32_t AvDdiReleaseSwizzlingRange(void *context, const void *request);
int32_t AvDdiSetPalette(void *context, const void *request);
int32_t AvDdiStopCapture(void *context, const void *request);
int32_t AvDdiCreateOverlay(void *context, void *request);
int32_t AvDdiQueryVidPnHWCapability(void *context, av::DxgkargQueryVidPnHwCapabilityRecord *request);
int32_t AvDdiSetPowerComponentFState(void *context, uint32_t component, uint32_t fstate);
int32_t AvDdiQueryDependentEngineGroup(void *context, av::DxgkargQueryDependentEngineGroupRecord *request);
int32_t AvDdiQueryEngineStatus(void *context, av::DxgkargQueryEngineStatusRecord *request);
int32_t AvDdiResetEngine(void *context, av::DxgkargResetEngineRecord *request);
int32_t AvDdiStopDeviceAndReleasePostDisplayOwnership(void *context, uint32_t target,
                                                      av::DxgkDisplayInformationRecord *display);
int32_t AvDdiSystemDisplayEnable(void *context, uint32_t target, void *flags, uint32_t *width, uint32_t *height,
                                 int32_t *format);
void    AvDdiSystemDisplayWrite(void *context, const void *source, uint32_t width, uint32_t height, uint32_t stride,
                                uint32_t x, uint32_t y);
int32_t AvDdiCancelCommand(void *context, const av::DxgkargCancelCommandRecord *request);
int32_t AvDdiGetChildContainerId(void *context, uint32_t child, av::DxgkChildContainerIdRecord *container);
int32_t AvDdiPowerRuntimeControlRequest(void *context, const void *control_code, const void *input, size_t input_size,
                                        void *output, size_t output_size, size_t *returned);
int32_t AvDdiSetVidPnSourceAddressWithMultiPlaneOverlay(void *context, const av::DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord *request);
int32_t AvDdiNotifySurpriseRemoval(void *context, int32_t removal_type);
int32_t AvDdiGetNodeMetadata(void *context, uint32_t node_and_adapter, av::DxgkNodeMetadataRecord *metadata);
int32_t AvDdiSetPowerPState(void *context, uint32_t component, uint32_t pstate);
int32_t AvDdiControlInterrupt2(void *context, av::DxgkargControlInterrupt2Record control);
int32_t AvDdiCalibrateGpuClock(void *context, uint32_t node, uint32_t engine, av::DxgkGpuClockDataRecord *clock);
int32_t AvDdiSubmitCommandVirtual(void *context, const av::DxgkargSubmitCommandVirtualRecord *request);
void    AvDdiSetRootPageTable(void *context, const av::DxgkargSetRootPageTableRecord *request);
size_t  AvDdiGetRootPageTableSize(void *context, av::DxgkargGetRootPageTableSizeRecord *request);
int32_t AvDdiCreateProcess(void *context, av::DxgkargCreateProcessRecord *request);
int32_t AvDdiDestroyProcess(void *context, void *process);
void    AvDdiSetStablePowerState(void *context, const av::DxgkargSetStablePowerStateRecord *request);
int32_t AvDdiControlModeBehavior(void *context, av::DxgkargControlModeBehaviorRecord *request);
int32_t AvDdiUpdateMonitorLinkInfo(void *context, av::DxgkargUpdateMonitorLinkInfoRecord *request);
int32_t AvDdiCreateHwQueue(void *context, av::DxgkargCreateHwQueueRecord *request);
/* The doorbell DDIs (feature 049): each names a hardware queue or a doorbell,
 * and every one of them reaches the adapter behind it. */
/* The native fences WDDM 3.1 asks for beside the doorbells (feature 049). */
int32_t AvDdiQueryFeatureSupport(void *context, av::DxgkargQueryFeatureSupportRecord *request);
int32_t AvDdiCreateNativeFence(void *context, av::DxgkargCreateNativeFenceRecord *request);
int32_t AvDdiDestroyNativeFence(void *context, const av::DxgkargDestroyNativeFenceRecord *request);
int32_t AvDdiUpdateMonitoredValues(void *context, const av::DxgkargUpdateNativeFenceValuesRecord *request);
int32_t AvDdiUpdateCurrentValuesFromCpu(void *context, const av::DxgkargUpdateNativeFenceValuesRecord *request);
int32_t AvDdiCreateDoorbell(av::DxgkargCreateDoorbellRecord *request);
int32_t AvDdiConnectDoorbell(av::DxgkargConnectDoorbellRecord *request);
int32_t AvDdiDisconnectDoorbell(const av::DxgkargDisconnectDoorbellRecord *request);
int32_t AvDdiDestroyDoorbell(const av::DxgkargDestroyDoorbellRecord *request);
int32_t AvDdiNotifyWorkSubmission(const av::DxgkargNotifyWorkSubmissionRecord *request);
int32_t AvDdiDestroyHwQueue(void *queue);
int32_t AvDdiSubmitCommandToHwQueue(void *context, const av::DxgkargSubmitCommandToHwQueueRecord *request);
int32_t AvDdiResetHwEngine(void *context, const av::DxgkargResetHwEngineRecord *request);
int32_t AvDdiResumeHwEngine(void *context, const av::DxgkargResumeHwEngineRecord *request);
/* An overlay handle this driver never creates: nothing behind it to reach. */
int32_t AvDdiUpdateOverlay(void *overlay, const void *request);
int32_t AvDdiFlipOverlay(void *overlay, const void *request);
int32_t AvDdiDestroyOverlay(void *overlay);
/* The KSYNCHRONIZE_ROUTINE DxgkCbSynchronizeExecution runs at device IRQL. */
uint8_t AvSynchronizedNotify(void *notification);
/* The platform timer's EXT_CALLBACK, at DISPATCH. */
void AvVsyncTick(void *timer, void *vsync);

}

#endif  // AV_DDI_ENTRY_POINTS_HPP
