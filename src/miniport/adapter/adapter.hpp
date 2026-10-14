/* One PCI display device, from DxgkDdiAddDevice to DxgkDdiRemoveDevice. Each
 * DDI method answers the NTSTATUS dxgkrnl receives. */
#ifndef AV_ADAPTER_HPP
#define AV_ADAPTER_HPP

#include "proto/dma/present_operation.hpp"
#include "miniport/adapter/adapter_description.hpp"
#include "miniport/adapter/driver_capabilities.hpp"
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

class PresentSurface;
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
    /* Harmless when not started: dxgkrnl stops an adapter whose start failed. */
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
    /* What this driver implements of a feature the OS is staging (feature 049,
     * research.md §18): dxgkrnl leaves a feature disabled unless the driver
     * answers for it, and a capability behind a disabled feature fails the
     * adapter start. */
    int32_t query_feature_support(DxgkargQueryFeatureSupportRecord *request);
    /* Answers at any adapter state: capabilities describe the driver. */
    int32_t query_adapter_info(const DxgkargQueryAdapterInfoRecord *query);
    /* A host-visible chunk binding (feature 045), a host window chunk
     * (docs/host-memory.md), a tail publish, or avinspect.exe's trace escape
     * (docs/trace.md), told apart by the record's magic. */
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
    int32_t patch(const DxgkargPatchRecord *request);
    /* DISPATCH: queues the fence and asks for the DPC that completes it. */
    int32_t submit_command(const DxgkargSubmitCommandRecord *request);
    int32_t preempt_command(const DxgkargPreemptCommandRecord *request);
    int32_t query_current_fence(DxgkargQueryCurrentFenceRecord *request);
    int32_t build_paging_buffer(DxgkargBuildPagingBufferRecord *request);
    /* TDR: every packet is abandoned and the device reset, until restart. */
    int32_t reset_from_timeout();
    int32_t restart_from_timeout();
    int32_t is_supported_vidpn(DxgkargIsSupportedVidPnRecord *request);
    /* Declines: dxgkrnl then builds the VidPn from the modes offered. */
    int32_t recommend_functional_vidpn(const DxgkargRecommendFunctionalVidPnRecord *request);
    int32_t enum_vidpn_cofunc_modality(const DxgkargEnumVidPnCofuncModalityRecord *request);
    int32_t commit_vidpn(const DxgkargCommitVidPnRecord *request);
    /* DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH is the path and nothing else. */
    int32_t update_active_vidpn_present_path(const D3dkmdtVidPnPresentPathRecord *path);
    int32_t recommend_monitor_modes(const DxgkargRecommendMonitorModesRecord *request);
    /* Declines: dxgkrnl then joins the one source to the one target itself. */
    int32_t recommend_vidpn_topology(const DxgkargRecommendVidPnTopologyRecord *request);
    int32_t get_scan_line(DxgkargGetScanLineRecord *request);
    /* CRTC_VSYNC starts and stops the synthetic vsync. */
    int32_t control_interrupt(int32_t type, uint8_t enable);
    /* With FlipOnVSyncMmIo, at device IRQL synchronized with the interrupt. */
    int32_t set_vidpn_source_address(const DxgkargSetVidPnSourceAddressRecord *request);
    int32_t set_vidpn_source_visibility(const DxgkargSetVidPnSourceVisibilityRecord *request);
    int32_t present(void *context, DxgkargPresentRecord *request);
    int32_t set_pointer_shape(const DxgkargSetPointerShapeRecord *request);
    int32_t set_pointer_position(const DxgkargSetPointerPositionRecord *request);
    /* The legacy surface answers as dxgkrnl's own defaults for an empty slot:
     * no swizzling range, palette, capture or overlay exists to act on. */
    int32_t acquire_swizzling_range(void *request);
    int32_t release_swizzling_range(const void *request);
    int32_t set_palette(const void *request);
    int32_t stop_capture(const void *request);
    int32_t create_overlay(void *request);
    /* The driver itself rotates, scales, clones and converts nothing. */
    int32_t query_vidpn_hw_capability(DxgkargQueryVidPnHwCapabilityRecord *request);
    /* Per-engine recovery for the one node and engine, as basicrender.sys. */
    int32_t query_dependent_engine_group(DxgkargQueryDependentEngineGroupRecord *request);
    int32_t query_engine_status(DxgkargQueryEngineStatusRecord *request);
    int32_t reset_engine(DxgkargResetEngineRecord *request);
    /* A buffer rendered but never submitted holds nothing of the driver's. */
    int32_t cancel_command(const DxgkargCancelCommandRecord *request);
    /* The device is gone: nothing touches it again, and stopping does not wait on it. */
    int32_t notify_surprise_removal(int32_t removal_type);
    /* Stops the device; the scan-out is guest pages behind a GPU address, so no
     * frame buffer is handed to the basic display driver. */
    int32_t stop_device_and_release_post_display_ownership(uint32_t target, DxgkDisplayInformationRecord *display);
    int32_t get_child_container_id(uint32_t child, DxgkChildContainerIdRecord *container);
    /* No frame buffer to write at HIGH_LEVEL: dxgkrnl falls back to the POST adapter. */
    int32_t system_display_enable(uint32_t target, void *flags, uint32_t *width, uint32_t *height, int32_t *format);
    void    system_display_write(const void *source, uint32_t width, uint32_t height, uint32_t stride, uint32_t x,
                                 uint32_t y);
    /* No power component is reported. */
    int32_t set_power_component_fstate(uint32_t component, uint32_t fstate);
    int32_t power_runtime_control_request(const void *control_code, const void *input, size_t input_size,
                                          void *output, size_t output_size, size_t *returned);
    int32_t set_vidpn_source_address_with_multi_plane_overlay(const DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord *request);
    int32_t set_vidpn_source_address_with_multi_plane_overlay3(DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record *request);
    int32_t check_multi_plane_overlay_support3(DxgkargCheckMultiPlaneOverlaySupport3Record *request);
    int32_t get_multi_plane_overlay_caps(DxgkargGetMultiPlaneOverlayCapsRecord *request);
    int32_t get_post_composition_caps(DxgkargGetPostCompositionCapsRecord *request);
    /* One 3D node named 3DNode00, as basicrender.sys names it, with a GPU MMU. */
    int32_t get_node_metadata(uint32_t node_and_adapter, DxgkNodeMetadataRecord *metadata);
    int32_t set_power_pstate(uint32_t component, uint32_t pstate);
    /* ControlInterrupt with a vsync state; a timer has no phase to keep. */
    int32_t control_interrupt2(DxgkargControlInterrupt2Record control);
    /* The GPU clock is the CPU performance counter. */
    int32_t calibrate_gpu_clock(uint32_t node, uint32_t engine, DxgkGpuClockDataRecord *clock);
    /* SubmitCommand for a context on GPU virtual addresses: the fence, queued. */
    int32_t submit_command_virtual(const DxgkargSubmitCommandVirtualRecord *request);
    /* dxgkrnl ignores what it returns; a root for no context is logged. */
    void    set_root_page_table(const DxgkargSetRootPageTableRecord *request);
    size_t  get_root_page_table_size(DxgkargGetRootPageTableSizeRecord *request);
    /* No per-process driver state, as basicrender.sys keeps none. */
    int32_t create_process(DxgkargCreateProcessRecord *request);
    int32_t destroy_process(void *process);
    /* Nothing to hold steady: the device has no power states. */
    void    set_stable_power_state(const DxgkargSetStablePowerStateRecord *request);
    /* dxgkrnl asks on every start, before any capability query. */
    int32_t control_mode_behavior(DxgkargControlModeBehaviorRecord *request);
    int32_t update_monitor_link_info(DxgkargUpdateMonitorLinkInfoRecord *request);
    /* Hardware queues: reached only once hardware scheduling is on. */
    /* The native fences WDDM 3.1 asks for beside the doorbells (feature 049):
     * the OS owns the memory, this driver owns which fences exist. */
    int32_t create_native_fence(DxgkargCreateNativeFenceRecord *request);
    int32_t destroy_native_fence(const DxgkargDestroyNativeFenceRecord *request);
    int32_t update_monitored_values(const DxgkargUpdateNativeFenceValuesRecord *request);
    int32_t update_current_values_from_cpu(const DxgkargUpdateNativeFenceValuesRecord *request);
    int32_t create_hw_queue(void *context, DxgkargCreateHwQueueRecord *request);
    int32_t destroy_hw_queue(void *queue);
    /* In flight until the DPC writes its progress fence. */
    int32_t submit_command_to_hw_queue(const DxgkargSubmitCommandToHwQueueRecord *request);
    /* The doorbells a process submits through (feature 049): created on a
     * hardware queue, connected to the page that queue's context was bound to,
     * and disconnected when dxgkrnl suspends, resets or migrates the device. */
    int32_t create_doorbell(DxgkargCreateDoorbellRecord *request);
    int32_t connect_doorbell(DxgkargConnectDoorbellRecord *request);
    int32_t disconnect_doorbell(const DxgkargDisconnectDoorbellRecord *request);
    int32_t destroy_doorbell(const DxgkargDestroyDoorbellRecord *request);
    /* A process that stored to a doorbell whose status asked it to tell the
     * driver. This device's never does (research.md §7), so this is the
     * fallback's fallback: it publishes nothing and answers success, because
     * the work is already in the ring and the store already woke the worker. */
    int32_t notify_work_submission(const DxgkargNotifyWorkSubmissionRecord *request);
    int32_t reset_hw_engine(const DxgkargResetHwEngineRecord *request);
    /* After a successful reset; submissions were never refused in between. */
    int32_t resume_hw_engine(const DxgkargResumeHwEngineRecord *request);

    /* This adapter and everything it holds, as the trace escape's state dump;
     * never waits for a lock. */
    void report(StateWriter &writer) override;

    bool invariants_hold() const;

private:
    /* The pixels of a present nothing else executes (feature 046): the
     * sub-rectangles [first, end), or the whole rectangle for a present with
     * none. The ticket its packet waits for, 0 when they have already moved. */
    uint64_t move_presented_pixels(const DxgkargPresentRecord &request, PresentOperation kind, uint32_t first,
                                   uint32_t end);
    /* The present's private data names `ticket`, so its packet waits for the
     * pixels (PresentTicketRecord); nothing for 0 or no room. */
    void     mark_present(DxgkargPresentRecord &request, uint64_t ticket);
    /* UMDRIVERPRIVATE: which adapter this is, and the features the user-mode
     * driver may use (UmdAdapterInfo), in as many words as the caller asked. */
    [[nodiscard]] Status answer_umd_private(const AdapterInfoQuery &request);
    /* What moves a present's pixels when neither the engine nor the device's
     * present copy is there. */
    PixelMover &immediate_mover();
    /* The host frame a blt from `source` comes from: the source allocation's
     * own shared id, at the point its private driver data names
     * (PresentPointRecord); naming nothing for a source the host holds no
     * surface of, or no point. */
    SurfacePoint image_of(const DxgkargPresentRecord &request, const DeviceAllocation *source);
    void log_refused_present(const DxgkargPresentRecord &request, bool operation_named);
    void log_unmoved_present(const char *why, const DxgkargPresentRecord &request, uint32_t sources,
                             uint32_t destinations);
    /* A present whose pixels the engine moves later but whose packet could
     * not be told to wait for them: whatever reads the destination after the
     * packet completes reads it before they land. */
    void log_unticketed_present(const DxgkargPresentRecord &request);
    void log_unreadable_surfaces(const PresentSurface &read, const PresentSurface &written, AllocationKind source_kind,
                                 AllocationKind destination_kind, const Rectangle &from, const Rectangle &into);

    static constexpr uint32_t config_space_bytes = 256u;
    static constexpr uint32_t adapter_device_uid = 0xFFFFFFFFu;  /* DISPLAY_ADAPTER_HW_ID */
    enum : int32_t { power_unspecified = 0, power_d0 = 1, power_d3 = 4 };
    enum : int32_t {   /* DXGKQAITYPE_* */
        query_umd_private = 0,
        query_driver_caps = 1,
        query_segment4 = 11,
        query_gpu_mmu_caps = 13,
        query_page_table_level_desc = 14,
        query_physical_adapter_caps = 15,
        query_display_driver_caps_extension = 16,
        query_wddm_device_caps = 29,
    /* DXGKQAITYPE_NATIVE_FENCE_CAPS and DXGKQAITYPE_USERMODESUBMISSION_CAPS
     * (d3dkmddi.h:1857), feature 049. */
    query_native_fence_caps = 37,
    query_user_mode_submission_caps = 38,
    };
    enum : int32_t { removal_hibernation = 0, removal_pnp_notify = 1 };   /* DXGK_SURPRISE_REMOVAL_TYPE */
    enum : int32_t { vsync_enable = 0, vsync_disable_no_phase = 2 };      /* DXGK_CRTC_VSYNC_STATE */
    enum : int32_t { engine_type_3d = 1 };                                /* DXGK_ENGINE_TYPE_3D */
    enum : int32_t { interrupt_crtc_vsync = 3 };                          /* DXGK_INTERRUPT_CRTC_VSYNC */
    /* The source the vertical blanks report: target 0 shows it. */
    static constexpr uint32_t vsync_source = 0u;
    static constexpr uint32_t engine_responsive = 1u;   /* DXGK_ENGINESTATUS.Responsive */
    static constexpr uint32_t system_memory_driver_segment = 0u;
    /* VIRTIO_PCI_ISR_CONFIG, and the message the device's configuration vector
     * raises when the driver was granted one per source. */
    static constexpr uint8_t  isr_configuration_changed = 0x2u;
    static constexpr uint32_t config_message = 0u;
    /* The shared memory capability locating virtio-gpu-av-pci's doorbell
     * pages on BAR 0 (docs/protocol.md). */
    static constexpr uint8_t  doorbell_region = 0x72u;

    [[nodiscard]] Status discover_and_map(const DxgkrnlInterfaceRecord *dxgkrnl_interface);
    /* The doorbell pages, when the device has them: a device without them is
     * one no av context can run on, and runs everything else as before. */
    [[nodiscard]] Status map_doorbells(const PciBaseAddresses &bars);
    /* GUID_WDDM_INTERFACE_FEATURE: fills the caller's table with this
     * driver's feature entry points. */
    [[nodiscard]] Status answer_feature_interface(QueryInterfaceRecord &query);
    /* Which side of the boundary publishes the tail of the context a doorbell
     * hangs off, told to the context itself (feature 049). */
    void                 submissions_to_user_mode(const HwQueue &queue);
    void                 submissions_to_driver(void *doorbell);
    /* CreateContext for a context that asked with a record: the host context
     * set up on memory the driver owns and maps into the process (feature 043;
     * docs/protocol.md, "Context memory"). */
    [[nodiscard]] Status bind_av(Context &context);
    /* The device a teardown can still ask, or null when it cannot. */
    VirtioGpuDevice     *answering_device();
    /* Tells the device of every shared object a destroy forgot. PASSIVE. */
    void                 release_shared();
    int32_t          window_chunk(const DxgkargEscapeRecord &escape);
    int32_t          context_views(const DxgkargEscapeRecord &escape);
    int32_t          wake_event(const DxgkargEscapeRecord &escape);
    int32_t          next_point(const DxgkargEscapeRecord &escape);
    int32_t          publish_own_tail(const DxgkargEscapeRecord &escape);
    /* SubmitCommandVirtual for an av context: published, rung, and held on
     * the node until the fence page says the host finished it. */
    [[nodiscard]] Status submit_av(const DxgkargSubmitCommandVirtualRecord &request, uint64_t &awaited);
    [[nodiscard]] Status submit_follow(const Context &context, const FollowRecord &follow, uint32_t fence,
                                       void *&owner, uint64_t &awaited);
    /* A present's packet: held on the node until the present engine finished
     * its ticket. Whether the private data named one of the queue's. */
    bool                 submit_present(const DxgkargSubmitCommandVirtualRecord &request, Status &status,
                                        void *&owner, uint64_t &awaited);
    /* StartDevice: which device this adapter is running on, so the one it wakes
     * up to can be told apart from it. A device that will not say leaves
     * nothing to compare, and the adapter runs as it did before. */
    void                 remember_device();
    /* The device is going away, is already gone, or is not the one the region
     * was found on: no chunk is placed in its window from now on. */
    void                 withdraw_window();
    /* D0: a guest restored onto another device cannot finish what it holds, so
     * the work queued on the one that is gone is reported lost (FR-005). */
    void                 judge_device();
    [[nodiscard]] Status power_up();
    void                 start_copier();
    /* A device just started that offers the present engine: the engine's
     * page registered, and presents queued to it from now on. */
    void                 start_presents();
    /* A device just started: the display clock's page registered with it. */
    void                 attach_clock();
    /* Which sources have a committed mode: what a power transition puts back. */
    uint32_t             scanning_out() const;
    /* Configures each source the transition held, logging any it cannot: a
     * source that does not come back is one dark display, not a failed resume. */
    void                 restore_scanouts();
    [[nodiscard]] Status power_down();
    /* One line naming a DDI and the status it answered, spent from the budget. */
    /* The host's scanout follows what was committed: the mode's resource, or
     * nothing when the path is gone. */
    [[nodiscard]] Status follow_commit(uint32_t source);
    /* The displays the device says it has, as the sources the driver offers. */
    void                 follow_output_count(uint32_t outputs);
    /* A device whose displays are not all the same size, which the one mode
     * list this driver offers cannot describe. */
    void                 log_unlike_outputs();
    /* The device's own display info, followed into the mode list and the EDID.
     * Passive only: it waits for the device's answer. */
    [[nodiscard]] Status follow_display_info();
    /* GET_DISPLAY_MODES, or, from a device without them, the standard modes
     * that fit GET_DISPLAY_INFO's rectangle. */
    [[nodiscard]] Status follow_display_modes();
    void                 log_offered_modes(uint32_t offered);
    /* DPC: a display event taken, and Windows told to read each monitor again. */
    void                 announce_display_change(bool running);
    /* DPC, after dxgkrnl has been told what it completed: the primaries sent
     * to the host. */
    void                 refresh_scanout(uint64_t now);
    /* The same, but only once for each configuration change the device
     * signalled: the interrupt notices, and the next DDI that describes the
     * monitor asks, because asking waits and the DPC may not. */
    void                 follow_display_change();
    void                 log_status(const char *step, int32_t status);
    /* The same line for a failure only: a DDI dxgkrnl calls on every frame.
     * Each line is a port write per byte, every one an exit that waits for
     * QEMU's main loop, and the thread that waits is the one presenting. */
    void                 log_failure(const char *step, int32_t status);
    void                 log_allocation(const DxgkargCreateAllocationRecord &request, int32_t status);
    /* Which source dxgkrnl showed or hid. */
    void                 log_visibility(const DxgkargSetVidPnSourceVisibilityRecord &request, int32_t status);
    /* Where dxgkrnl points a source, which is the only place the scanout can
     * look for a frame. Skipped at device IRQL, where the budget's lock is
     * illegal; dxgkrnl only calls there for an MMIO flip on vsync, which this
     * driver does not report. */
    /* A primary shown on `source`: what SetVidPnSourceAddress and a multiplane
     * flip's base plane both come to. */
    int32_t              show_primary(uint32_t source, uint32_t segment, int64_t address, void *allocation);
    /* The flip's answer to dxgkrnl, and the DPC that carries it asked for. */
    int32_t              flipped(Status set);
    void                 log_scanout(uint32_t source, uint32_t segment, int64_t address);
    void                 log_refused_overlay(const DxgkargCheckMultiPlaneOverlaySupport3Record &request);
    /* A step whose answer is only meaningful next to the handle it was given -
     * DxgkDdiCreateDevice receives the runtime's handle and answers with the
     * driver's, and a guest that ends up with neither needs both in the log. */
    void                 log_handle(const char *step, const void *handle, int32_t status);

    Platform                      &platform_;
    LogBudget                      budget_;
    void                          *physical_device_object_;
    DxgkrnlInterface               dxgkrnl_;
    ResourceList                   resources_;
    BarMappings                    mappings_;
    VirtioPciLayout                layout_;
    ObjectStorage<VirtioGpuDevice> device_;
    /* What moves a present's pixels while the device cannot: the miniport,
     * through its own uncached view of the pages. */
    MappedPixels                   mapped_pixels_;
    /* What moves them while it can: made with the device and gone with it. */
    ObjectStorage<PresentCopier>   copier_;
    /* What hands them to the device's own thread instead, where it offers
     * one: made with the device, under the next epoch, and gone with it. */
    ObjectStorage<PresentQueue>    presents_;
    uint32_t                       present_epochs_;
    ObjectStorage<MappedRegisterWindow> doorbells_;
    /* The doorbells processes hold on hardware queues (feature 049): made with
     * the mapping they hand out pages of, and gone with it. */
    ObjectStorage<DoorbellTable>   doorbell_table_;
    DriverCapabilities             capabilities_;
    GpuMmuGeometry                 geometry_;
    ApertureSegment                segment_;
    HostVisibleSegment             host_visible_;
    MemorySegments                 memory_;
    /* Before the av contexts, whose window chunks are placed in it. */
    HostWindow                     window_;
    DisplayInfoResponse            display_info_;
    DisplayModeList                modes_;
    Edid                           edid_;
    DisplaySources                 sources_;
    OverlaySupport                 overlays_;
    /* Before the allocation table, which mints into it, and before the device
     * table, which opens from it (feature 053). */
    SharedObjectTable              shared_;
    AllocationTable                allocations_;
    DeviceTable                    devices_;
    ContextTable                   contexts_;
    AvContextTable                av_contexts_;
    HwQueueTable                   hw_queues_;
    HwQueueSubmissions             hw_submissions_;
    /* What each hardware queue runs on, which a submission at DISPATCH reads
     * without taking the queue table's mutex (feature 049). */
    HwQueueContexts                hw_queue_contexts_;
    NativeFenceTable               native_fences_;
    /* Whether this OS lets the adapter be hardware scheduled, and lets a
     * process submit for itself on it, asked once each at StartDevice: a node
     * that reports what the OS has not enabled fails the whole adapter start
     * (feature 049, research.md §15). */
    bool                           hardware_scheduling_;
    bool                           user_mode_submission_;
    ExecutionNode                  node_;
    EngineProgress                 progress_;
    PowerTransition                transition_;
    DeviceIdentity                 identity_;
    AdapterDescription             description_;
    InterruptNotification          notification_;
    /* Held across the DPC: notification_'s record and presented_fence_ are
     * one per adapter, and fences are reported in the order they were taken. */
    SpinLock                       dpc_lock_;
    DeviceVblank                   vsync_;
    HardwareCursor                 cursor_;
    DisplayScanout                 scanout_;
    /* The node fence the host was last shown a frame for. */
    uint32_t                       presented_fence_;
    /* Present DDIs so far, and as many as the DPC has shown. A present writes
     * the primary without moving it, so the scanout is flushed on the next
     * two completions after one - the DMA buffer it wrote is among them -
     * and not on every completion, each of which cost the host a full-frame
     * readback under the device lock every session's commands wait on. */
    uint64_t                       presents_made_;
    uint64_t                       presents_shown_;
    uint32_t                       refreshes_owed_;
    uint64_t                       refreshed_at_;
    bool                           refresh_waiting_;
    /* Rows of pixels this miniport moved for a present nothing executes - the
     * desktop's (feature 046, FR-004) - and presents it could not move. */
    uint64_t                       present_rows_;
    uint64_t                       presents_unmoved_;
    /* Presents the engine moves whose packets had no room to name their
     * ticket, and so complete without waiting for the pixels. */
    uint64_t                       presents_unticketed_;
    /* Set from the interrupt when the device says its configuration changed,
     * cleared by the DDI that follows it. */
    uint64_t                       display_changed_;
    /* Set with display_changed_ by the interrupt, taken by the DPC, which
     * clears the device's event and has Windows replug each monitor. */
    uint64_t                       display_announced_;
    bool                           started_;
    /* DXGK_START_INFO.AdapterLuid, HighPart above LowPart: what the user-mode
     * driver is told, through UMDRIVERPRIVATE, so it talks to this adapter
     * and not the first avgpu D3DKMT enumerates. */
    uint64_t                       luid_;
    bool                           present_;

    friend struct AdapterProbe;
};

}  // namespace av

#endif  // AV_ADAPTER_HPP
