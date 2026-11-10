#ifndef AV_ADAPTER_HPP
#define AV_ADAPTER_HPP

#include "proto/dma/present_operation.hpp"
#include "miniport/adapter/adapter_description.hpp"
#include "miniport/adapter/driver_capabilities.hpp"
#include "miniport/adapter/gpu_clock_origin.hpp"
#include "miniport/ddi/dxgkarg_control_interrupt2_record.hpp"
#include "miniport/display/display_info_response.hpp"
#include "miniport/display/display_mode_list.hpp"
#include "miniport/display/display_scanout.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/display/overlay_support.hpp"
#include "miniport/display/edid.hpp"
#include "miniport/display/hardware_cursor.hpp"
#include "miniport/display/device_vblank.hpp"
#include "miniport/memory/allocation_table.hpp"
#include "miniport/memory/shared_object_table.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/host_visible_segment.hpp"
#include "miniport/memory/host_window.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/memory/gpu_mmu_geometry.hpp"
#include "miniport/driver/dxgkrnl_interface.hpp"
#include "miniport/driver/interrupt_notification.hpp"
#include "core/platform/spin_lock.hpp"
#include "miniport/virtio/bar_mappings.hpp"
#include "miniport/pci/resource_list.hpp"
#include "miniport/present/mapped_pixels.hpp"
#include "miniport/present/pixel_mover.hpp"
#include "miniport/present/present_copier.hpp"
#include "miniport/present/present_queue.hpp"
#include "miniport/present/present_waiters.hpp"
#include "core/platform/log_budget.hpp"
#include "core/platform/platform.hpp"
#include "miniport/scheduling/context_table.hpp"
#include "miniport/scheduling/device_table.hpp"
#include "miniport/adapter/power_transition.hpp"
#include "miniport/adapter/device_identity.hpp"
#include "miniport/av_context/av_context_table.hpp"
#include "miniport/pci/mapped_register_window.hpp"
#include "miniport/scheduling/engine_progress.hpp"
#include "miniport/scheduling/execution_node.hpp"
#include "miniport/scheduling/hw_queue_contexts.hpp"
#include "miniport/scheduling/native_fence_table.hpp"
#include "miniport/scheduling/render_km_tally.hpp"
#include "miniport/scheduling/hw_queue_submissions.hpp"
#include "miniport/scheduling/doorbell_table.hpp"
#include "miniport/scheduling/hw_queue_table.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_reporter.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "miniport/virtio/virtio_pci_layout.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AdapterInfoQuery;
class DeviceAllocation;
class ReleaseRecord;
class PointView;

class PresentSurface;
class SurfacePair;
class SurfacePoint;
class Rectangle;

struct D3dkmdtVidPnPresentPathRecord;
struct DxgkChildContainerIdRecord;
struct DxgkChildDescriptorRecord;
struct DxgkDisplayInformationRecord;
struct DxgkGpuClockDataRecord;
struct DxgkNodeMetadataRecord;
struct DxgkChildStatusRecord;
struct DxgkDeviceDescriptorRecord;
struct DxgkrnlInterfaceRecord;
struct DxgkargCancelCommandRecord;
struct DxgkargCloseAllocationRecord;
struct DxgkargQueryDependentEngineGroupRecord;
struct DxgkargQueryEngineStatusRecord;
struct DxgkargResetEngineRecord;
struct DxgkargCommitVidPnRecord;
struct DxgkargEnumVidPnCofuncModalityRecord;
struct DxgkargGetScanLineRecord;
struct DxgkargIsSupportedVidPnRecord;
struct DxgkargRecommendFunctionalVidPnRecord;
struct DxgkargRecommendMonitorModesRecord;
struct DxgkargRecommendVidPnTopologyRecord;
struct DxgkargBuildPagingBufferRecord;
struct DxgkargCollectDbgInfoRecord;
struct DxgkargCreateContextRecord;
struct DxgkargCreateAllocationRecord;
struct DxgkargCreateDeviceRecord;
struct DxgkargDescribeAllocationRecord;
struct DxgkargDestroyAllocationRecord;
struct DxgkargGetStandardAllocationDriverDataRecord;
struct DxgkargOpenAllocationRecord;
struct DxgkargPatchRecord;
struct DxgkargPreemptCommandRecord;
struct DxgkargPresentRecord;
struct DxgkargSetPointerPositionRecord;
struct DxgkargSetPointerShapeRecord;
struct DxgkargSetVidPnSourceAddressRecord;
struct DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord;
struct DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record;
struct DxgkargCheckMultiPlaneOverlaySupport3Record;
struct DxgkargGetMultiPlaneOverlayCapsRecord;
struct DxgkargGetPostCompositionCapsRecord;
struct DxgkargSetVidPnSourceVisibilityRecord;
struct DxgkargQueryCurrentFenceRecord;
struct DxgkargQueryVidPnHwCapabilityRecord;
struct DxgkargRenderRecord;
struct DxgkargSetDisplayPrivateDriverFormatRecord;
struct DxgkargSubmitCommandRecord;
struct DxgkargSubmitCommandVirtualRecord;
struct DxgkargSetRootPageTableRecord;
struct DxgkargGetRootPageTableSizeRecord;
struct DxgkargCreateProcessRecord;
struct DxgkargSetStablePowerStateRecord;
struct DxgkargControlModeBehaviorRecord;
struct DxgkargUpdateMonitorLinkInfoRecord;
struct DxgkargCreateHwQueueRecord;
struct DxgkargCreateNativeFenceRecord;
struct DxgkargQueryFeatureSupportRecord;
struct QueryInterfaceRecord;
struct DxgkargDestroyNativeFenceRecord;
struct DxgkargUpdateNativeFenceValuesRecord;
struct DxgkargSubmitCommandToHwQueueRecord;
struct DxgkargResetHwEngineRecord;
struct DxgkargResumeHwEngineRecord;
struct DxgkargEscapeRecord;
struct DxgkargQueryAdapterInfoRecord;
struct LinkedDeviceRecord;
struct AdapterProbe;
class Context;
class FollowRecord;
class PciBaseAddresses;
class StateWriter;

class Adapter final : public StateReporter {
public:
    Adapter(Platform &platform, void *physical_device_object);
    ~Adapter();
    Adapter(const Adapter &) = delete;
    Adapter &operator=(const Adapter &) = delete;

    void *physical_device_object() const;

    int32_t start_device(void *start_info, const DxgkrnlInterfaceRecord *dxgkrnl_interface,
                         uint32_t *sources, uint32_t *children);
    int32_t stop_device();
    int32_t dispatch_io_request(uint32_t source, void *request);
    uint8_t interrupt_routine(uint32_t message);
    void    dpc_routine();
    int32_t query_child_relations(DxgkChildDescriptorRecord *relations, uint32_t size);
    int32_t query_child_status(DxgkChildStatusRecord *status, uint8_t non_destructive);
    int32_t query_device_descriptor(uint32_t child, DxgkDeviceDescriptorRecord *descriptor);
    int32_t set_power_state(uint32_t device, int32_t power_state, int32_t action);
    int32_t notify_acpi_event(int32_t type, uint32_t event, void *argument, uint32_t *acpi_flags);
    void    reset_device();
    int32_t query_interface(void *query);
    int32_t query_feature_support(DxgkargQueryFeatureSupportRecord *request);
    int32_t query_adapter_info(const DxgkargQueryAdapterInfoRecord *query);
    int32_t escape(const DxgkargEscapeRecord *escape);
    int32_t collect_dbg_info(const DxgkargCollectDbgInfoRecord *collect);
    int32_t create_device(DxgkargCreateDeviceRecord *request);
    int32_t destroy_device(void *device);
    int32_t create_allocation(DxgkargCreateAllocationRecord *request);
    int32_t destroy_allocation(const DxgkargDestroyAllocationRecord *request);
    int32_t describe_allocation(DxgkargDescribeAllocationRecord *request);
    int32_t get_standard_allocation_driver_data(DxgkargGetStandardAllocationDriverDataRecord *request);
    int32_t open_allocation(void *device, DxgkargOpenAllocationRecord *request);
    int32_t close_allocation(void *device, const DxgkargCloseAllocationRecord *request);
    int32_t set_display_private_driver_format(const DxgkargSetDisplayPrivateDriverFormatRecord *request);
    int32_t create_context(void *device, DxgkargCreateContextRecord *request);
    int32_t destroy_context(void *context);
    int32_t render(void *context, DxgkargRenderRecord *request);
    int32_t render_km(void *context, DxgkargRenderRecord *request);
    int32_t patch(const DxgkargPatchRecord *request);
    int32_t submit_command(const DxgkargSubmitCommandRecord *request);
    int32_t preempt_command(const DxgkargPreemptCommandRecord *request);
    int32_t query_current_fence(DxgkargQueryCurrentFenceRecord *request);
    int32_t build_paging_buffer(DxgkargBuildPagingBufferRecord *request);
    int32_t reset_from_timeout();
    int32_t restart_from_timeout();
    int32_t is_supported_vidpn(DxgkargIsSupportedVidPnRecord *request);
    int32_t recommend_functional_vidpn(const DxgkargRecommendFunctionalVidPnRecord *request);
    int32_t enum_vidpn_cofunc_modality(const DxgkargEnumVidPnCofuncModalityRecord *request);
    int32_t commit_vidpn(const DxgkargCommitVidPnRecord *request);
    int32_t update_active_vidpn_present_path(const D3dkmdtVidPnPresentPathRecord *path);
    int32_t recommend_monitor_modes(const DxgkargRecommendMonitorModesRecord *request);
    int32_t recommend_vidpn_topology(const DxgkargRecommendVidPnTopologyRecord *request);
    int32_t get_scan_line(DxgkargGetScanLineRecord *request);
    int32_t control_interrupt(int32_t type, uint8_t enable);
    int32_t set_vidpn_source_address(const DxgkargSetVidPnSourceAddressRecord *request);
    int32_t set_vidpn_source_visibility(const DxgkargSetVidPnSourceVisibilityRecord *request);
    int32_t present(void *context, DxgkargPresentRecord *request);
    int32_t set_pointer_shape(const DxgkargSetPointerShapeRecord *request);
    int32_t set_pointer_position(const DxgkargSetPointerPositionRecord *request);
    int32_t acquire_swizzling_range(void *request);
    int32_t release_swizzling_range(const void *request);
    int32_t set_palette(const void *request);
    int32_t stop_capture(const void *request);
    int32_t create_overlay(void *request);
    int32_t query_vidpn_hw_capability(DxgkargQueryVidPnHwCapabilityRecord *request);
    int32_t query_dependent_engine_group(DxgkargQueryDependentEngineGroupRecord *request);
    int32_t query_engine_status(DxgkargQueryEngineStatusRecord *request);
    int32_t reset_engine(DxgkargResetEngineRecord *request);
    int32_t cancel_command(const DxgkargCancelCommandRecord *request);
    int32_t notify_surprise_removal(int32_t removal_type);
    int32_t stop_device_and_release_post_display_ownership(uint32_t target, DxgkDisplayInformationRecord *display);
    int32_t get_child_container_id(uint32_t child, DxgkChildContainerIdRecord *container);
    int32_t system_display_enable(uint32_t target, void *flags, uint32_t *width, uint32_t *height, int32_t *format);
    void    system_display_write(const void *source, uint32_t width, uint32_t height, uint32_t stride, uint32_t x,
                                 uint32_t y);
    int32_t set_power_component_fstate(uint32_t component, uint32_t fstate);
    int32_t power_runtime_control_request(const void *control_code, const void *input, size_t input_size,
                                          void *output, size_t output_size, size_t *returned);
    int32_t set_vidpn_source_address_with_multi_plane_overlay(const DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord *request);
    int32_t set_vidpn_source_address_with_multi_plane_overlay3(DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record *request);
    int32_t check_multi_plane_overlay_support3(DxgkargCheckMultiPlaneOverlaySupport3Record *request);
    int32_t get_multi_plane_overlay_caps(DxgkargGetMultiPlaneOverlayCapsRecord *request);
    int32_t get_post_composition_caps(DxgkargGetPostCompositionCapsRecord *request);
    int32_t get_node_metadata(uint32_t node_and_adapter, DxgkNodeMetadataRecord *metadata);
    int32_t set_power_pstate(uint32_t component, uint32_t pstate);
    int32_t control_interrupt2(DxgkargControlInterrupt2Record control);
    int32_t calibrate_gpu_clock(uint32_t node, uint32_t engine, DxgkGpuClockDataRecord *clock);
    int32_t submit_command_virtual(const DxgkargSubmitCommandVirtualRecord *request);
    void    set_root_page_table(const DxgkargSetRootPageTableRecord *request);
    size_t  get_root_page_table_size(DxgkargGetRootPageTableSizeRecord *request);
    int32_t create_process(DxgkargCreateProcessRecord *request);
    int32_t destroy_process(void *process);
    void    set_stable_power_state(const DxgkargSetStablePowerStateRecord *request);
    int32_t control_mode_behavior(DxgkargControlModeBehaviorRecord *request);
    int32_t update_monitor_link_info(DxgkargUpdateMonitorLinkInfoRecord *request);
    int32_t create_native_fence(DxgkargCreateNativeFenceRecord *request);
    int32_t destroy_native_fence(const DxgkargDestroyNativeFenceRecord *request);
    int32_t update_monitored_values(const DxgkargUpdateNativeFenceValuesRecord *request);
    int32_t update_current_values_from_cpu(const DxgkargUpdateNativeFenceValuesRecord *request);
    int32_t create_hw_queue(void *context, DxgkargCreateHwQueueRecord *request);
    int32_t destroy_hw_queue(void *queue);
    int32_t submit_command_to_hw_queue(const DxgkargSubmitCommandToHwQueueRecord *request);
    int32_t create_doorbell(DxgkargCreateDoorbellRecord *request);
    int32_t connect_doorbell(DxgkargConnectDoorbellRecord *request);
    int32_t disconnect_doorbell(const DxgkargDisconnectDoorbellRecord *request);
    int32_t destroy_doorbell(const DxgkargDestroyDoorbellRecord *request);
    int32_t notify_work_submission(const DxgkargNotifyWorkSubmissionRecord *request);
    int32_t reset_hw_engine(const DxgkargResetHwEngineRecord *request);
    int32_t resume_hw_engine(const DxgkargResumeHwEngineRecord *request);

    void report(StateWriter &writer) override;

    bool invariants_hold() const;

private:
    uint64_t move_presented_pixels(const DxgkargPresentRecord &request, PresentOperation kind, uint32_t first,
                                   uint32_t end);
    void     mark_present(DxgkargPresentRecord &request, uint64_t ticket);
    [[nodiscard]] Status answer_umd_private(const AdapterInfoQuery &request);
    PixelMover &immediate_mover();
    SurfacePair  surfaces_of(const DeviceAllocation *source, const DeviceAllocation *destination,
                             const SurfacePoint &image, bool filling) const;
    SurfacePoint image_of(const DxgkargPresentRecord &request, const DeviceAllocation *source,
                          const PointView &points);
    bool         point_missing(const DxgkargPresentRecord &request, const DeviceAllocation *source,
                               const PointView &points);
    void log_refused_present(const DxgkargPresentRecord &request, bool operation_named);
    void log_unmoved_present(const char *why, const DxgkargPresentRecord &request, uint32_t sources,
                             uint32_t destinations);
    void log_unticketed_present(const DxgkargPresentRecord &request);
    void log_unreadable_surfaces(const PresentSurface &read, const PresentSurface &written, AllocationKind source_kind,
                                 AllocationKind destination_kind, const Rectangle &from, const Rectangle &into);

    static constexpr uint32_t config_space_bytes = 256u;
    static constexpr uint32_t adapter_device_uid = 0xFFFFFFFFu;
    enum : int32_t { power_unspecified = 0, power_d0 = 1, power_d3 = 4 };
    enum : int32_t {
        query_umd_private = 0,
        query_driver_caps = 1,
        query_segment4 = 11,
        query_gpu_mmu_caps = 13,
        query_page_table_level_desc = 14,
        query_physical_adapter_caps = 15,
        query_display_driver_caps_extension = 16,
        query_wddm_device_caps = 29,
    query_native_fence_caps = 37,
    query_user_mode_submission_caps = 38,
    };
    enum : int32_t { removal_hibernation = 0, removal_pnp_notify = 1 };
    enum : int32_t { vsync_enable = 0, vsync_disable_no_phase = 2 };
    enum : int32_t { engine_type_3d = 1 };
    enum : int32_t { interrupt_crtc_vsync = 3 };
    static constexpr uint32_t vsync_source = 0u;
    static constexpr uint32_t engine_responsive = 1u;
    static constexpr uint32_t system_memory_driver_segment = 0u;
    static constexpr uint8_t  isr_configuration_changed = 0x2u;
    static constexpr uint32_t config_message = 0u;
    static constexpr uint8_t  doorbell_region = 0x72u;

    [[nodiscard]] Status discover_and_map(const DxgkrnlInterfaceRecord *dxgkrnl_interface);
    [[nodiscard]] Status map_doorbells(const PciBaseAddresses &bars);
    [[nodiscard]] Status answer_feature_interface(QueryInterfaceRecord &query);
    void                 submissions_to_user_mode(const HwQueue &queue);
    void                 submissions_to_driver(void *doorbell);
    [[nodiscard]] Status bind_av(Context &context);
    VirtioGpuDevice     *answering_device();
    void                 release_shared();
    int32_t          window_chunk(const DxgkargEscapeRecord &escape);
    int32_t          context_views(const DxgkargEscapeRecord &escape);
    int32_t          wake_event(const DxgkargEscapeRecord &escape);
    int32_t          clock_origin(const DxgkargEscapeRecord &escape);
    int32_t          next_point(const DxgkargEscapeRecord &escape);
    int32_t          pages_read(const DxgkargEscapeRecord &escape);
    int32_t          publish_own_tail(const DxgkargEscapeRecord &escape);
    [[nodiscard]] Status submit_av(const DxgkargSubmitCommandVirtualRecord &request, uint64_t &awaited);
    [[nodiscard]] Status submit_follow(const Context &context, const FollowRecord &follow, uint32_t fence,
                                       void *&owner, uint64_t &awaited);
    [[nodiscard]] Status submit_release(const Context &context, const ReleaseRecord &release, uint32_t fence,
                                        void *&owner, uint64_t &awaited);
    bool                 submit_present(const DxgkargSubmitCommandVirtualRecord &request, Status &status,
                                        void *&owner, uint64_t &awaited);
    void                 remember_device();
    void                 withdraw_window();
    void                 judge_device();
    [[nodiscard]] Status power_up();
    void                 start_copier();
    void                 start_presents();
    void                 attach_clock();
    void                 announce_window();
    uint32_t             scanning_out() const;
    void                 restore_scanouts();
    [[nodiscard]] Status power_down();
    [[nodiscard]] Status follow_commit(uint32_t source);
    void                 retime_display(uint32_t source, const DisplayMode &mode);
    void                 follow_output_count(uint32_t outputs);
    void                 log_unlike_outputs();
    [[nodiscard]] Status follow_display_info();
    [[nodiscard]] Status follow_display_modes();
    void                 log_offered_modes(uint32_t offered);
    void                 announce_display_change(bool running);
    void                 refresh_scanout(uint64_t now);
    void                 follow_display_change();
    void                 log_status(const char *step, int32_t status);
    void                 log_failure(const char *step, int32_t status);
    void                 log_refused_gdi_surface(const DxgkargGetStandardAllocationDriverDataRecord &request);
    void                 log_allocation(const DxgkargCreateAllocationRecord &request, int32_t status);
    void                 log_visibility(const DxgkargSetVidPnSourceVisibilityRecord &request, int32_t status);
    int32_t              show_primary(uint32_t source, uint32_t segment, int64_t address, void *allocation);
    int32_t              flipped(Status set);
    void                 log_scanout(uint32_t source, uint32_t segment, int64_t address);
    void                 log_refused_overlay(const DxgkargCheckMultiPlaneOverlaySupport3Record &request);
    void                 log_handle(const char *step, const void *handle, int32_t status);

    Platform                      &platform_;
    LogBudget                      budget_;
    void                          *physical_device_object_;
    DxgkrnlInterface               dxgkrnl_;
    ResourceList                   resources_;
    BarMappings                    mappings_;
    VirtioPciLayout                layout_;
    ObjectStorage<VirtioGpuDevice> device_;
    MappedPixels                   mapped_pixels_;
    ObjectStorage<PresentCopier>   copier_;
    ObjectStorage<PresentQueue>    presents_;
    uint32_t                       present_epochs_;
    ObjectStorage<MappedRegisterWindow> doorbells_;
    ObjectStorage<DoorbellTable>   doorbell_table_;
    DriverCapabilities             capabilities_;
    GpuMmuGeometry                 geometry_;
    ApertureSegment                segment_;
    HostVisibleSegment             host_visible_;
    MemorySegments                 memory_;
    HostWindow                     window_;
    DisplayInfoResponse            display_info_;
    DisplayModeList                modes_;
    Edid                           edid_;
    DisplaySources                 sources_;
    OverlaySupport                 overlays_;
    SharedObjectTable              shared_;
    AllocationTable                allocations_;
    DeviceTable                    devices_;
    ContextTable                   contexts_;
    RenderKmTally                  render_km_;
    AvContextTable                av_contexts_;
    HwQueueTable                   hw_queues_;
    HwQueueSubmissions             hw_submissions_;
    HwQueueContexts                hw_queue_contexts_;
    NativeFenceTable               native_fences_;
    bool                           hardware_scheduling_;
    bool                           user_mode_submission_;
    ExecutionNode                  node_;
    PresentWaiters                 present_waiters_;
    EngineProgress                 progress_;
    PowerTransition                transition_;
    DeviceIdentity                 identity_;
    AdapterDescription             description_;
    InterruptNotification          notification_;
    SpinLock                       dpc_lock_;
    GpuClockOrigin                 clock_origin_;
    DeviceVblank                   vsync_;
    HardwareCursor                 cursor_;
    DisplayScanout                 scanout_;
    uint32_t                       presented_fence_;
    uint64_t                       presents_made_;
    uint64_t                       presents_shown_;
    uint32_t                       refreshes_owed_;
    uint64_t                       refreshed_at_;
    bool                           refresh_waiting_;
    uint64_t                       present_rows_;
    uint64_t                       presents_unmoved_;
    uint64_t                       presents_unticketed_;
    uint64_t                       presents_ticketed_;
    uint64_t                       presents_parked_;
    uint64_t                       presents_refused_;
    uint64_t                       presents_unpointed_;
    uint32_t                       last_present_flags_;
    int32_t                        last_present_status_;
    uint32_t                       last_present_destination_kind_;
    uint64_t                       display_changed_;
    uint64_t                       display_announced_;
    uint64_t                       scanout_period_;
    bool                           started_;
    uint64_t                       luid_;
    bool                           present_;

    friend struct AdapterProbe;
};

}

#endif
