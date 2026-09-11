/* One PCI display device, from DxgkDdiAddDevice to DxgkDdiRemoveDevice. Each
 * DDI method answers the NTSTATUS dxgkrnl receives. */
#ifndef AV_ADAPTER_HPP
#define AV_ADAPTER_HPP

#include "allocation_table.hpp"
#include "aperture_segment.hpp"
#include "bar_mappings.hpp"
#include "context_table.hpp"
#include "device_table.hpp"
#include "display_mode_list.hpp"
#include "display_sources.hpp"
#include "driver_capabilities.hpp"
#include "edid.hpp"
#include "execution_node.hpp"
#include "hardware_cursor.hpp"
#include "interrupt_notification.hpp"
#include "dxgkrnl_interface.hpp"
#include "object_storage.hpp"
#include "platform.hpp"
#include "resource_list.hpp"
#include "status.hpp"
#include "synthetic_vsync.hpp"
#include "virtio_gpu_device.hpp"
#include "virtio_pci_layout.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct D3dkmdtVidPnPresentPathRecord;
struct DxgkChildContainerIdRecord;
struct DxgkChildDescriptorRecord;
struct DxgkDisplayInformationRecord;
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
struct DxgkargSetVidPnSourceVisibilityRecord;
struct DxgkargQueryCurrentFenceRecord;
struct DxgkargQueryVidPnHwCapabilityRecord;
struct DxgkargRenderRecord;
struct DxgkargSetDisplayPrivateDriverFormatRecord;
struct DxgkargSubmitCommandRecord;
struct DxgkargEscapeRecord;
struct DxgkargQueryAdapterInfoRecord;
struct LinkedDeviceRecord;
struct AdapterProbe;

class Adapter {
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
    int32_t link_device(void *physical_device_object, LinkedDeviceRecord *linked);
    /* Answers at any adapter state: capabilities describe the driver. */
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
    /* At most DISPATCH: the vsync tick reads the address it sets. */
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
    int32_t set_vidpn_source_address_with_multi_plane_overlay(const void *request);

    bool invariants_hold() const;

private:
    static constexpr uint32_t config_space_bytes = 256u;
    static constexpr uint32_t adapter_device_uid = 0xFFFFFFFFu;  /* DISPLAY_ADAPTER_HW_ID */
    enum : int32_t { power_unspecified = 0, power_d0 = 1, power_d3 = 4 };
    enum : int32_t { query_driver_caps = 1, query_segment4 = 11, query_wddm_device_caps = 29 };   /* DXGKQAITYPE_* */
    enum : int32_t { removal_hibernation = 0, removal_pnp_notify = 1 };   /* DXGK_SURPRISE_REMOVAL_TYPE */
    enum : int32_t { interrupt_crtc_vsync = 3 };                          /* DXGK_INTERRUPT_CRTC_VSYNC */
    static constexpr uint32_t engine_responsive = 1u;   /* DXGK_ENGINESTATUS.Responsive */
    enum : int32_t { transformation_identity = 1 };   /* D3DKMDT_VPPS_IDENTITY, D3DKMDT_VPPR_IDENTITY */

    [[nodiscard]] Status discover_and_map(const DxgkrnlInterfaceRecord *dxgkrnl_interface);
    [[nodiscard]] Status power_up();
    [[nodiscard]] Status power_down();

    Platform                      &platform_;
    void                          *physical_device_object_;
    DxgkrnlInterface               dxgkrnl_;
    ResourceList                   resources_;
    BarMappings                    mappings_;
    VirtioPciLayout                layout_;
    ObjectStorage<VirtioGpuDevice> device_;
    DriverCapabilities             capabilities_;
    ApertureSegment                segment_;
    DisplayModeList                modes_;
    Edid                           edid_;
    DisplaySources                 sources_;
    AllocationTable                allocations_;
    DeviceTable                    devices_;
    ContextTable                   contexts_;
    ExecutionNode                  node_;
    InterruptNotification          notification_;
    SyntheticVsync                 vsync_;
    HardwareCursor                 cursor_;
    bool                           started_;
    bool                           present_;

    friend struct AdapterProbe;
};

}  // namespace av

#endif  // AV_ADAPTER_HPP
