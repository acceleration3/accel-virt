#include "core/adapter/adapter.hpp"

#include "core/adapter/debug_report.hpp"
#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/ddi/dxgk_child_container_id_record.hpp"
#include "core/ddi/dxgk_child_descriptor_record.hpp"
#include "core/ddi/dxgk_child_status_record.hpp"
#include "core/ddi/dxgk_device_descriptor_record.hpp"
#include "core/ddi/dxgk_device_info_record.hpp"
#include "core/ddi/dxgk_display_information_record.hpp"
#include "core/ddi/dxgk_gpu_clock_data_record.hpp"
#include "core/ddi/dxgk_node_metadata_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/ddi/dxgkarg_cancel_command_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "core/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "core/ddi/dxgkarg_create_process_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_escape_record.hpp"
#include "proto/resources/host_visible_bind_record.hpp"
#include "core/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "core/ddi/dxgkarg_get_scan_line_record.hpp"
#include "core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/ddi/dxgkarg_patch_record.hpp"
#include "core/ddi/dxgkarg_preempt_command_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "core/ddi/dxgkarg_query_dependent_engine_group_record.hpp"
#include "core/ddi/dxgkarg_query_engine_status_record.hpp"
#include "core/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
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
#include "core/ddi/dxgkarg_set_vidpn_source_visibility_record.hpp"
#include "core/ddi/dxgkarg_submit_command_record.hpp"
#include "core/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "core/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "core/ddi/linked_device_record.hpp"
#include "core/display/monitor_modes.hpp"
#include "core/memory/paging_request.hpp"
#include "core/memory/standard_allocation.hpp"
#include "core/miniport/adapter_info_query.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/present/present_request.hpp"
#include "core/rvk/capset_exchange.hpp"
#include "core/scheduling/patch_request.hpp"
#include "core/scheduling/render_request.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/text_buffer.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "core/support/scope_exit.hpp"
#include "core/vidpn/cofunc_modality.hpp"
#include "core/vidpn/path_transformation.hpp"
#include "core/vidpn/vidpn_commit.hpp"
#include "core/vidpn/vidpn_support_check.hpp"
#include "core/virtio/virtio_capability_walk.hpp"
#include "core/virtio/virtio_region.hpp"

namespace av {

namespace {

/* What basicdisplay.sys reports for its one output (docs/ddi-sequences.md). */
enum : int32_t {
    type_video_output         = 1,   /* TypeVideoOutput */
    output_technology_other   = -1,  /* D3DKMDT_VOT_OTHER */
    orientation_awareness_none = 1,  /* D3DKMDT_MOA_NONE */
    hpd_interruptible         = 4,   /* HpdAwarenessInterruptible */
    status_connection         = 1,   /* StatusConnection */
    status_rotation           = 2,   /* StatusRotation */
};

}  // namespace

Adapter::Adapter(Platform &platform, void *physical_device_object)
    : platform_(platform), budget_(platform), physical_device_object_(physical_device_object), dxgkrnl_(platform),
      resources_(platform), mappings_(platform, dxgkrnl_), layout_(), device_(), doorbells_(), capabilities_(platform), geometry_(platform),
      segment_(platform), host_visible_(platform), region_blob_(platform),
      memory_(platform, segment_, host_visible_),
      display_info_(), modes_(), edid_(platform, modes_.preferred()), sources_(platform),
      allocations_(platform, segment_, memory_), devices_(platform, allocations_, dxgkrnl_), contexts_(platform, devices_, geometry_),
      rvk_contexts_(platform, dxgkrnl_, allocations_),
      hw_queues_(platform, contexts_), hw_submissions_(platform), node_(platform), progress_(platform, node_), transition_(), identity_(platform), notification_(platform, dxgkrnl_), vsync_(platform, dxgkrnl_, sources_),
      cursor_(platform, sources_, device_), scanout_(platform, budget_, device_), presented_fence_(0u),
      display_changed_(0u), started_(false), present_(true) {}

Adapter::~Adapter() { static_cast<void>(stop_device()); }

void *Adapter::physical_device_object() const
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::physical_device_object");
    return physical_device_object_;
}

Status Adapter::discover_and_map(const DxgkrnlInterfaceRecord *dxgkrnl_interface)
{
    Status status = dxgkrnl_.initialize(dxgkrnl_interface);
    DxgkDeviceInfoRecord info = {};
    if (status == Status::ok) status = dxgkrnl_.get_device_information(info);
    if (status == Status::ok) status = segment_.initialize(info.SystemMemorySize);
    if (status == Status::ok) status = resources_.initialize(info.TranslatedResourceList, ResourceList::max_bytes);
    uint8_t config[config_space_bytes] = {};
    if (status == Status::ok) status = dxgkrnl_.read_config_space(config, config_space_bytes);
    if (status == Status::ok) status = VirtioCapabilityWalk(config, config_space_bytes).walk(layout_);
    if (status == Status::ok)
        status = mappings_.map(PciBaseAddresses(config, config_space_bytes), resources_, layout_);
    if (status == Status::ok) status = map_doorbells(PciBaseAddresses(config, config_space_bytes));
    /* Where the device's host-visible region is, if it presents one; whether it
     * is offered waits for the capset (feature 041, FR-001). */
    if (status == Status::ok) host_visible_.discover(PciBaseAddresses(config, config_space_bytes), layout_);
    return status;
}

Status Adapter::map_doorbells(const PciBaseAddresses &bars)
{
    const VirtioRegion region = layout_.shared_memory(doorbell_region);
    if (!region.present()) return Status::ok;
    const Status mapped = mappings_.map_region(bars, resources_, region);
    if (mapped != Status::ok) return mapped;
    return doorbells_.construct(platform_, mappings_.address_of(region), region.length());
}

void Adapter::remember_device()
{
    CapsetInfo     capset;
    CapsetExchange exchange(platform_, device_.object());
    const Status   read = exchange.read(capset);
    if (read == Status::ok) {
        identity_.remember(capset);
        host_visible_.offer(capset);
        open_region();
    }
    log_status("rvkgpu: device capset", NtStatus(read).value());
}

/* A blob already made is the region working: a D0 that finds the device
 * still powered has nothing to make again. */
void Adapter::open_region()
{
    if (!host_visible_.offered() || region_blob_.created()) return;
    const Status created = region_blob_.create(device_.object(), HostVisibleBlob::first_resource_id,
                                               host_visible_.size());
    const Status mapped = created == Status::ok ? region_blob_.map(device_.object(), 0u) : created;
    log_status("rvkgpu: host-visible region", NtStatus(mapped).value());
    if (mapped != Status::ok) close_region();
}

/* The device is going away, is already gone, or would not give the driver its
 * region. Either way the adapter has no host memory: the segment stops being
 * offered, because a segment reported without storage behind it is one VidMm
 * places allocations in and then writes into nothing (docs/kb/04-memory.md). */
void Adapter::close_region()
{
    region_blob_.abandon();
    host_visible_.forget();
}

void Adapter::judge_device()
{
    if (!identity_.known()) return;
    CapsetInfo     capset;
    CapsetExchange exchange(platform_, device_.object());
    if (exchange.read(capset) != Status::ok) {
        close_region();
        return;
    }
    if (identity_.accept(capset)) {
        /* The same device, built again: its region holds nothing of what it
         * held before, so the blob is made again on the device that is there
         * now. */
        open_region();
        return;
    }
    /* Nothing runs on a device that changed, and its region is not the one
     * VidMm was told about. What an rvk context queued for it stays on the
     * node and never completes: the engine stops moving, and dxgkrnl's timeout
     * detection resets it through ResetEngine. Dropping it here would leave an
     * idle engine answering responsive while VidSch still waits on it, and
     * telling dxgkrnl would mean DMA_FAULTED, which bugchecks this adapter
     * (docs/kb/05-scheduling.md). The display is the scanout the transition
     * restored, none of this, and keeps working. */
    close_region();
    rvk_contexts_.abandon_all();
}

Status Adapter::power_up()
{
    /* The only refusal is storage already holding a device: already powered. */
    if (device_.construct(platform_, mappings_, layout_) != Status::ok) return Status::ok;
    const Status status = device_.object().start(resources_.message_interrupts());
    if (status != Status::ok) device_.destroy();
    cursor_.forget_device();
    scanout_.forget_device();
    return status;
}

/* A removed device cannot acknowledge a reset; destruction still abandons it. */
Status Adapter::power_down()
{
    if (!device_.constructed()) return Status::ok;
    /* A reset destroys every host context and blob, and the device object goes
     * with it. The region stays found and offered - VidMm keeps the segment -
     * for whatever brings the device back to make its blob again. */
    rvk_contexts_.abandon_all();
    region_blob_.abandon();
    const Status status = present_ ? device_.object().stop() : Status::ok;
    device_.destroy();
    return status;
}

int32_t Adapter::start_device(void *, const DxgkrnlInterfaceRecord *dxgkrnl_interface, uint32_t *sources,
                              uint32_t *children)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::start_device");
    InvariantScope<Adapter> guard(platform_, *this, "start_device");
    if (started_) return NtStatus(Status::invalid_state).value();
    if (dxgkrnl_interface == nullptr || sources == nullptr || children == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    present_ = true;
    ScopeExit undo([this] {
        cursor_.release();
        scanout_.release();
        vsync_.destroy();
        doorbells_.destroy();
        mappings_.unmap_all();
    });
    Status status = discover_and_map(dxgkrnl_interface);
    if (status == Status::ok) status = vsync_.initialize();
    if (status == Status::ok) status = cursor_.initialize();
    if (status == Status::ok) status = power_up();
    if (status != Status::ok) {
        char       bytes[72];
        TextBuffer message(bytes, sizeof bytes);
        message.append("rvkgpu: StartDevice failed, status 0x");
        message.append_hex(static_cast<uint32_t>(NtStatus(status).value()), 8u);
        platform_.log(message.text());
        return NtStatus(status).value();
    }
    undo.dismiss();
    follow_output_count(device_.object().scanouts());
    /* A device that will not say what its displays are leaves the standard
     * mode list standing, which is a working display, so this never fails the
     * start - but it is logged, because the modes offered are then not the
     * host's. */
    log_status("rvkgpu: display info", NtStatus(follow_display_info()).value());
    remember_device();
    *sources = sources_.count();
    *children = sources_.count();
    started_ = true;
    platform_.log("rvkgpu: StartDevice");
    return NtStatus(Status::ok).value();
}

int32_t Adapter::stop_device()
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_device");
    InvariantScope<Adapter> guard(platform_, *this, "stop_device");
    vsync_.destroy();
    const Status status = power_down();
    scanout_.release();
    cursor_.release();
    doorbells_.destroy();
    mappings_.unmap_all();
    identity_.forget();
    close_region();
    host_visible_.forget();
    started_ = false;
    const int32_t stopped = NtStatus(status).value();
    log_status("rvkgpu: StopDevice", stopped);
    return stopped;
}

int32_t Adapter::dispatch_io_request(uint32_t, void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::dispatch_io_request");
    return NtStatus(Status::not_supported).value();
}

/* A message interrupt is ours by construction; a line interrupt only if the
 * device set an ISR bit, since the line may be shared. */
/* A configuration change is how the device says its displays moved: with
 * message interrupts it is the config vector, and on a line interrupt it is a
 * bit of the ISR, which is read to clear and so must be noticed here. */
uint8_t Adapter::interrupt_routine(uint32_t message)
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::interrupt_routine");
    /* A device that is being built is not one to read from: power_up
     * constructs it and then starts it, and dxgkrnl leaves the interrupt
     * connected across the whole transition. */
    if (!device_.constructed() || !present_ || !device_.object().running()) return 0u;
    const bool    messages = resources_.message_interrupts() != 0u;
    const uint8_t isr = messages ? 0u : device_.object().read_isr();
    if (!messages && isr == 0u) return 0u;
    if (messages ? message == config_message : (isr & isr_configuration_changed) != 0u)
        platform_.store_atomic(display_changed_, 1u);
    static_cast<void>(dxgkrnl_.queue_dpc());
    return 1u;
}

void Adapter::dpc_routine()
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::dpc_routine");
    uint32_t completed = 0u;
    const bool harvest = device_.constructed() && present_ && device_.object().running();
    /* A device that fails every harvest would otherwise log on every DPC, so
     * each of these lines is spent from the budget (feature 037, FR-003). */
    if (harvest && device_.object().harvest_control(completed) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: control queue harvest failed in the DPC");
    if (harvest && device_.object().harvest_cursor(completed) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: cursor queue harvest failed in the DPC");
    rvk_contexts_.serve(node_);
    if (node_.complete(notification_) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: a completion was refused in the DPC");
    if (hw_submissions_.complete(notification_) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: a progress fence signal was refused in the DPC");
    /* A frame is carried when a submission completed since the last DPC, never
     * because the control queue answered: our own scanout requests complete
     * there, and taking those for presents would flush forever. */
    const uint32_t fence = node_.current_fence();
    const bool     presented = fence != presented_fence_;
    presented_fence_ = fence;
    if (harvest && scanout_.refresh(sources_, presented) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: a scanout refresh was refused in the DPC");
    dxgkrnl_.notify_dpc();
}

/* One video output per display the device has. dxgkrnl's array holds one
 * descriptor more than the children StartDevice reported, zeroed, so the buffer
 * is checked against that. */
int32_t Adapter::query_child_relations(DxgkChildDescriptorRecord *relations, uint32_t size)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_relations");
    if (relations == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (size < (sources_.count() + 1u) * sizeof *relations) return NtStatus(NtStatus::buffer_too_small).value();
    platform_.log("rvkgpu: QueryChildRelations");
    for (uint32_t child = 0u; child < sources_.count(); child++) {
        relations[child].ChildDeviceType = type_video_output;
        relations[child].InterfaceTechnology = output_technology_other;
        relations[child].MonitorOrientationAwareness = orientation_awareness_none;
        relations[child].SupportsSdtvModes = 0u;
        relations[child].HpdAwareness = hpd_interruptible;
        relations[child].AcpiUid = 0u;
        relations[child].ChildUid = child;
    }
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_child_status(DxgkChildStatusRecord *status, uint8_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_status");
    if (status == nullptr || status->ChildUid >= sources_.count()) return NtStatus(Status::invalid_parameter).value();
    if (status->Type == status_rotation) return NtStatus(Status::invalid_parameter).value();
    int32_t answered = NtStatus(Status::not_supported).value();
    if (status->Type == status_connection) {
        status->HotPlugConnected = 1u;
        answered = NtStatus(Status::ok).value();
    }
    log_status("rvkgpu: QueryChildStatus", answered);
    return answered;
}

int32_t Adapter::query_device_descriptor(uint32_t child, DxgkDeviceDescriptorRecord *descriptor)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_device_descriptor");
    if (child >= sources_.count() || descriptor == nullptr) return NtStatus(Status::invalid_parameter).value();
    follow_display_change();
    /* The monitor's mode set is built from this EDID, so a bring-up that never
     * reaches a committable VidPn needs the answer visible. */
    const int32_t copied = edid_.copy(*descriptor).value();
    log_status("rvkgpu: QueryDeviceDescriptor", copied);
    return copied;
}

int32_t Adapter::set_power_state(uint32_t device, int32_t power_state, int32_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_power_state");
    InvariantScope<Adapter> guard(platform_, *this, "set_power_state");
    if (device != adapter_device_uid) return NtStatus(Status::ok).value();
    if (!started_) return NtStatus(Status::invalid_state).value();
    if (power_state == power_d0) {
        platform_.log("rvkgpu: SetPowerState D0");
        const Status powered = power_up();
        log_status("rvkgpu: SetPowerState D0 device", NtStatus(powered).value());
        /* What power took away, put back: dxgkrnl kept every allocation and
         * VidPn across this, so what it expects to find is what was scanning
         * out before. */
        if (powered == Status::ok && transition_.held()) restore_scanouts();
        if (powered == Status::ok)
            log_status("rvkgpu: SetPowerState D0 vsync", NtStatus(vsync_.resume()).value());
        if (powered == Status::ok) judge_device();
        transition_.release();
        platform_.log("rvkgpu: SetPowerState D0 done");
        return NtStatus(powered).value();
    }
    if (power_state > power_d0 && power_state <= power_d3) {
        platform_.log("rvkgpu: SetPowerState D3");
        static_cast<void>(transition_.hold(device_.constructed(), scanning_out()));
        /* Before the device goes: a vertical blank reported to an adapter that
         * is not powered - or to one on its way out of S3, before dxgkrnl has
         * powered it up - is what bugchecks the guest. */
        vsync_.suspend();
        const int32_t down = NtStatus(power_down()).value();
        log_status("rvkgpu: SetPowerState D3 done", down);
        return down;
    }
    return NtStatus(Status::invalid_parameter).value();
}

/* dxgkrnl forwards ACPI events it does not handle itself; nothing to do. */
int32_t Adapter::notify_acpi_event(int32_t, uint32_t, void *, uint32_t *acpi_flags)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::notify_acpi_event");
    *acpi_flags = 0u;
    return NtStatus(Status::ok).value();
}

void Adapter::reset_device()
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::reset_device");
    if (device_.constructed()) device_.object().reset_immediately();
}

int32_t Adapter::query_interface(void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_interface");
    return NtStatus(Status::not_supported).value();
}

/* DpiAddDevice calls a registered LinkDevice after every AddDevice and fails
 * the device if it fails: answer a chain of one, led by this adapter. */
int32_t Adapter::link_device(void *, LinkedDeviceRecord *linked)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::link_device");
    if (linked == nullptr) return NtStatus(Status::invalid_parameter).value();
    linked->ChainUid = 0u;
    linked->NumberOfLinksInChain = 1u;
    linked->LeadLink = 1u;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_adapter_info(const DxgkargQueryAdapterInfoRecord *query)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_adapter_info");
    if (query == nullptr) return NtStatus(Status::invalid_parameter).value();
    const AdapterInfoQuery request(platform_, *query);
    int32_t                answered = NtStatus(NtStatus::not_implemented).value();
    switch (request.type()) {
    case query_driver_caps:
        answered = NtStatus(capabilities_.report_driver_caps(request)).value();
        break;
    case query_wddm_device_caps:
        answered = NtStatus(capabilities_.report_wddm_device_caps(request)).value();
        break;
    case query_segment4:
        answered = NtStatus(memory_.report(request)).value();
        break;
    case query_gpu_mmu_caps:
        answered = NtStatus(geometry_.report_caps(request)).value();
        break;
    case query_page_table_level_desc:
        answered = NtStatus(geometry_.report_level(request)).value();
        break;
    case query_physical_adapter_caps:
        answered = NtStatus(capabilities_.report_physical_adapter_caps(request, dxgkrnl_.device_handle())).value();
        break;
    case query_display_driver_caps_extension:
        answered = NtStatus(capabilities_.report_display_caps_extension(request)).value();
        break;
    default:
        break;
    }
    /* The order and outcome of these is what US1 asks the guest log to show;
     * dxgkrnl asks for types the driver declines, so the status is the point. */
    if (budget_.allows()) {
        char       bytes[80];
        TextBuffer message(bytes, sizeof bytes);
        message.append("rvkgpu: QueryAdapterInfo type ");
        message.append_decimal(static_cast<uint32_t>(request.type()));
        message.append(" status 0x");
        message.append_hex(static_cast<uint32_t>(answered), 8u);
        platform_.log(message.text());
    }
    return answered;
}

/* No escape is defined until a user-mode component needs one; basicrender
 * answers an escape it does not know the same way. */
int32_t Adapter::escape(const DxgkargEscapeRecord *escape)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::escape");
    if (escape == nullptr || escape->pPrivateDriverData == nullptr || escape->PrivateDriverDataSize == 0u)
        return NtStatus(Status::invalid_parameter).value();
    return bind_chunk(*escape);
}

/* The one escape this driver defines: a process binding a host-visible chunk to
 * its rvk context (feature 045). Every refusal is STATUS_INVALID_PARAMETER, a
 * status dxgkrnl hands the caller as it is (feature 037). */
int32_t Adapter::bind_chunk(const DxgkargEscapeRecord &escape)
{
    auto *const       bytes = static_cast<uint8_t *>(escape.pPrivateDriverData);
    uint32_t          magic = 0u;
    if (escape.PrivateDriverDataSize >= sizeof magic) platform_.copy(&magic, bytes, sizeof magic);
    if (magic != HostVisibleBindRecord::magic || escape.hContext == nullptr)
        return NtStatus(Status::not_supported).value();
    HostVisibleBindRecord record;
    VirtioGpuDevice *const device = answering_device();
    if (escape.PrivateDriverDataSize != HostVisibleBindRecord::encoded_size ||
        record.decode(bytes, escape.PrivateDriverDataSize) != Status::ok || device == nullptr ||
        rvk_contexts_.bind_chunk(escape.hContext, record, *device) != Status::ok)
        return NtStatus(Status::invalid_parameter).value();
    static_cast<void>(record.encode(bytes, HostVisibleBindRecord::encoded_size));   /* the length was checked */
    return NtStatus(Status::ok).value();
}

int32_t Adapter::collect_dbg_info(const DxgkargCollectDbgInfoRecord *collect)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::collect_dbg_info");
    if (collect == nullptr || (collect->pBuffer == nullptr && collect->BufferSize != 0u))
        return NtStatus(Status::invalid_parameter).value();
    if (collect->pExtension != nullptr) platform_.zero(collect->pExtension, sizeof *collect->pExtension);
    const bool powered = device_.constructed();
    const DebugReport report(platform_, started_, powered, powered ? device_.object().features() : 0u, node_);
    static_cast<void>(report.write(collect->pBuffer, collect->BufferSize));
    return NtStatus(Status::ok).value();
}

int32_t Adapter::create_device(DxgkargCreateDeviceRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_device");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const void   *runtime = request->hDevice;
    const int32_t created = NtStatus(devices_.create(*this, *request)).value();
    log_handle("rvkgpu: CreateDevice runtime", runtime, created);
    return created;
}

int32_t Adapter::destroy_device(void *device)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_device");
    return NtStatus(devices_.destroy(device)).value();
}

int32_t Adapter::create_allocation(DxgkargCreateAllocationRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t created = NtStatus(allocations_.create(*request)).value();
    log_status("rvkgpu: CreateAllocation", created);
    return created;
}

/* A source showing one of these must stop before the allocation goes: the DPC
 * follows the handle to find the primary's pages. */
int32_t Adapter::destroy_allocation(const DxgkargDestroyAllocationRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (request->pAllocationList != nullptr)
        for (uint32_t index = 0u; index < request->NumAllocations; index++) {
            sources_.forget_primary(request->pAllocationList[index]);
            /* The device writes a context's fence page and reads its ring
             * until the context is destroyed on it; pages freed first would be
             * written after they are someone else's. */
            void *const user = rvk_contexts_.user_of(allocations_.find(request->pAllocationList[index]));
            if (user != nullptr) static_cast<void>(rvk_contexts_.unbind(user, answering_device()));
        }
    return NtStatus(allocations_.destroy(*request)).value();
}

int32_t Adapter::describe_allocation(DxgkargDescribeAllocationRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::describe_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t described = NtStatus(allocations_.describe(*request, sources_)).value();
    log_status("rvkgpu: DescribeAllocation", described);
    return described;
}

int32_t Adapter::get_standard_allocation_driver_data(DxgkargGetStandardAllocationDriverDataRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_standard_allocation_driver_data");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t described = NtStatus(StandardAllocation(platform_, *request).describe()).value();
    log_status("rvkgpu: GetStandardAllocationDriverData", described);
    return described;
}

int32_t Adapter::open_allocation(void *device, DxgkargOpenAllocationRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::open_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(devices_.open(device, *request)).value();
}

int32_t Adapter::close_allocation(void *device, const DxgkargCloseAllocationRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::close_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(devices_.close(device, *request)).value();
}

int32_t Adapter::set_display_private_driver_format(const DxgkargSetDisplayPrivateDriverFormatRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_display_private_driver_format");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (request->PrimaryAllocation != nullptr && allocations_.find(request->PrimaryAllocation) == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    const int32_t set =
        NtStatus(sources_.set_private_format(request->VidPnSourceId, request->PrivateDriverFormatAttribute)).value();
    log_status("rvkgpu: SetDisplayPrivateDriverFormat", set);
    return set;
}

int32_t Adapter::create_context(void *device, DxgkargCreateContextRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_context");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    /* Nothing new runs on a device this adapter did not start on. */
    if (identity_.changed()) return NtStatus(Status::not_supported).value();
    const Status created = contexts_.create(device, *request);
    Context *const context = created == Status::ok ? contexts_.find(request->hContext) : nullptr;
    if (context == nullptr || !context->is_rvk()) return NtStatus(created).value();
    const Status bound = bind_rvk(*context);
    if (bound == Status::ok) return NtStatus(Status::ok).value();
    static_cast<void>(contexts_.destroy(request->hContext));
    request->hContext = nullptr;
    /* dxgkrnl passes a CreateContext status it does not expect straight to the
     * process and logs it as invalid; the reason is in the driver's log. */
    return NtStatus(Status::invalid_parameter).value();
}

Status Adapter::bind_rvk(Context &context)
{
    if (!doorbells_.constructed() || answering_device() == nullptr) return Status::not_supported;
    CapsetInfo     capset;
    CapsetExchange exchange(platform_, device_.object());
    const Status   read = exchange.read(capset);
    if (read != Status::ok) return read;
    return rvk_contexts_.bind(&context, context.rvk_record(), device_.object(), doorbells_.object(), capset);
}

VirtioGpuDevice *Adapter::answering_device()
{
    /* A device is only ever constructed running: power_up destroys one whose start failed. */
    return device_.constructed() && present_ ? &device_.object() : nullptr;
}

int32_t Adapter::destroy_context(void *context)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_context");
    /* DXGCONTEXT::DestroyContext destroys a context's queues first. */
    if (hw_queues_.serves(context)) return NtStatus(Status::invalid_parameter).value();
    const Context *const found = contexts_.find(context);
    if (found != nullptr && found->is_rvk()) {
        log_status("rvkgpu: rvk context destroyed", NtStatus(rvk_contexts_.unbind(context, answering_device())).value());
    }
    return NtStatus(contexts_.destroy(context)).value();
}

int32_t Adapter::render(void *context, DxgkargRenderRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::render");
    if (request == nullptr || contexts_.find(context) == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(RenderRequest(platform_, *request).render()).value();
}

int32_t Adapter::patch(const DxgkargPatchRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::patch");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(PatchRequest(platform_, *request).patch()).value();
}

int32_t Adapter::submit_command(const DxgkargSubmitCommandRecord *request)
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::submit_command");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    const Status status = node_.submit(request->SubmissionFenceId);
    if (status == Status::ok) static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(status).value();
}

int32_t Adapter::preempt_command(const DxgkargPreemptCommandRecord *request)
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::preempt_command");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    const Status status = node_.preempt(request->PreemptionFenceId);
    static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(status).value();
}

int32_t Adapter::query_current_fence(DxgkargQueryCurrentFenceRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_current_fence");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    request->CurrentFence = node_.current_fence();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::build_paging_buffer(DxgkargBuildPagingBufferRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::build_paging_buffer");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return PagingRequest(platform_, allocations_, budget_, *request).build().value();
}

int32_t Adapter::reset_from_timeout()
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::reset_from_timeout");
    InvariantScope<Adapter> guard(platform_, *this, "reset_from_timeout");
    node_.reset();
    hw_submissions_.abandon();
    return NtStatus(power_down()).value();
}

int32_t Adapter::restart_from_timeout()
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::restart_from_timeout");
    InvariantScope<Adapter> guard(platform_, *this, "restart_from_timeout");
    const Status status = started_ ? power_up() : Status::ok;
    if (status != Status::ok) return NtStatus(status).value();
    node_.restart();
    open_region();
    return NtStatus(status).value();
}

int32_t Adapter::is_supported_vidpn(DxgkargIsSupportedVidPnRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::is_supported_vidpn");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t decided = VidPnSupportCheck(platform_, dxgkrnl_, sources_, *request).decide().value();
    log_status("rvkgpu: IsSupportedVidPn", decided);
    return decided;
}

int32_t Adapter::recommend_functional_vidpn(const DxgkargRecommendFunctionalVidPnRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_functional_vidpn");
    const int32_t declined = NtStatus(NtStatus::no_recommended_functional_vidpn).value();
    log_status("rvkgpu: RecommendFunctionalVidPn", declined);
    return declined;
}

/* dxgkrnl repeats the VidPn DDIs while it settles a topology, so these lines
 * come out of the budget rather than one per call forever. */
void Adapter::log_handle(const char *step, const void *handle, int32_t status)
{
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append(step);
    message.append(" 0x");
    message.append_hex(reinterpret_cast<uint64_t>(handle), 16u);
    message.append(" status 0x");
    message.append_hex(static_cast<uint32_t>(status), 8u);
    platform_.log(message.text());
}

/* The device says how many displays it has; the driver offers a VidPn source
 * for each, up to what its arrays hold. A count it cannot serve is clamped, and
 * one of zero leaves the single source a display needs - either way the value
 * is logged, because the outputs the guest sees are then not the host's. */
void Adapter::follow_output_count(uint32_t outputs)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::follow_output_count");
    const Status status = sources_.set_count(outputs);
    if (status == Status::ok || !budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("rvkgpu: the device reports ");
    message.append_decimal(outputs);
    message.append(" displays, offering ");
    message.append_decimal(sources_.count());
    platform_.log(message.text());
}

Status Adapter::follow_display_info()
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::follow_display_info");
    if (!device_.constructed()) return Status::invalid_state;
    /* The request fits a max_request_size buffer, so the writer cannot refuse
     * it and its answer is voided as the scanout's are. */
    uint8_t          request[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.get_display_info());
    uint8_t  answer[GpuRequestWriter::max_response_size] = {};
    uint32_t written = 0u;
    const Status exchanged =
        device_.object().ask_control(request, writer.length(), answer, sizeof answer, written);
    if (exchanged != Status::ok) return exchanged;
    const Status decoded = display_info_.decode(answer, written);
    if (decoded != Status::ok) return decoded;
    const Status followed = modes_.follow(display_info_, 0u);
    if (followed != Status::ok) return followed;
    edid_.follow(modes_.preferred());
    log_unlike_outputs();
    return Status::ok;
}

/* Every output is offered the list the first scanout's rectangle allows, which
 * is right while the device gives them all the same rectangle - QEMU does. A
 * host that does not is worth a line, because the modes the other outputs are
 * offered are then not theirs. */
void Adapter::log_unlike_outputs()
{
    for (uint32_t source = 1u; source < sources_.count(); source++) {
        if (display_info_.width(source) == display_info_.width(0u) &&
            display_info_.height(source) == display_info_.height(0u))
            continue;
        if (!budget_.allows()) return;
        char       bytes[96];
        TextBuffer message(bytes, sizeof bytes);
        message.append("rvkgpu: display ");
        message.append_decimal(source);
        message.append(" is ");
        message.append_decimal(display_info_.width(source));
        message.append("x");
        message.append_decimal(display_info_.height(source));
        message.append(", not the first display's size");
        platform_.log(message.text());
        return;
    }
}

void Adapter::follow_display_change()
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::follow_display_change");
    if (platform_.load_atomic(display_changed_) == 0u) return;
    platform_.store_atomic(display_changed_, 0u);
    log_status("rvkgpu: display info", NtStatus(follow_display_info()).value());
}

void Adapter::log_status(const char *step, int32_t status)
{
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append(step);
    message.append(" status 0x");
    message.append_hex(static_cast<uint32_t>(status), 8u);
    platform_.log(message.text());
}

int32_t Adapter::enum_vidpn_cofunc_modality(const DxgkargEnumVidPnCofuncModalityRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::enum_vidpn_cofunc_modality");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t enumerated = CofuncModality(platform_, dxgkrnl_, budget_, modes_, sources_, *request).enumerate().value();
    log_status("rvkgpu: EnumVidPnCofuncModality", enumerated);
    return enumerated;
}

int32_t Adapter::commit_vidpn(const DxgkargCommitVidPnRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::commit_vidpn");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t applied = VidPnCommit(platform_, dxgkrnl_, modes_, sources_, *request).apply().value();
    if (applied >= 0) {
        const Status followed = follow_commit(request->AffectedVidPnSourceId);
        if (followed != Status::ok) log_status("rvkgpu: CommitVidPn scanout", NtStatus(followed).value());
    }
    log_status("rvkgpu: CommitVidPn", applied);
    return applied;
}

uint32_t Adapter::scanning_out() const
{
    uint32_t sources = 0u;
    for (uint32_t source = 0u; source < sources_.count(); source++)
        if (sources_.committed_mode(source) != nullptr) sources |= 1u << source;
    return sources;
}

void Adapter::restore_scanouts()
{
    for (uint32_t source = 0u; source < sources_.count(); source++) {
        if (!transition_.scanning_out(source)) continue;
        /* Said for every source power put back, because a resume that restored
         * one and not the other is the difference between a desktop and a
         * black screen. */
        log_status("rvkgpu: resume scanout", NtStatus(follow_commit(source)).value());
    }
}

Status Adapter::follow_commit(uint32_t source)
{
    const DisplayMode *mode = sources_.committed_mode(source);
    return mode != nullptr ? scanout_.configure(source, *mode) : scanout_.disable(source);
}

int32_t Adapter::update_active_vidpn_present_path(const D3dkmdtVidPnPresentPathRecord *path)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::update_active_vidpn_present_path");
    if (path == nullptr || path->VidPnSourceId >= sources_.count() ||
        path->VidPnTargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    return PathTransformation(*path).accepted().value();
}

int32_t Adapter::recommend_monitor_modes(const DxgkargRecommendMonitorModesRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_monitor_modes");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    follow_display_change();
    const int32_t recommended = MonitorModes(platform_, modes_, sources_, *request).recommend().value();
    log_status("rvkgpu: RecommendMonitorModes", recommended);
    return recommended;
}

int32_t Adapter::recommend_vidpn_topology(const DxgkargRecommendVidPnTopologyRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_vidpn_topology");
    const int32_t declined = NtStatus(NtStatus::no_recommended_vidpn_topology).value();
    log_status("rvkgpu: RecommendVidPnTopology", declined);
    return declined;
}

/* Target 0 shows source 0. */
int32_t Adapter::get_scan_line(DxgkargGetScanLineRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_scan_line");
    if (request == nullptr || request->VidPnTargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    const DisplayMode *mode = sources_.committed_mode(request->VidPnTargetId);
    if (mode == nullptr) return NtStatus(Status::invalid_state).value();
    const uint32_t line = mode->scan_line(platform_.interrupt_time());
    request->InVerticalBlank = line >= mode->height() ? 1u : 0u;
    request->ScanLine = line;
    return NtStatus(Status::ok).value();
}

/* Before StartDevice there is no timer to start. */
int32_t Adapter::control_interrupt(int32_t type, uint8_t enable)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::control_interrupt");
    if (type != interrupt_crtc_vsync) return NtStatus(Status::not_supported).value();
    const Status status = enable != 0u ? vsync_.enable() : Status::ok;
    if (enable == 0u) vsync_.disable();
    if (status == Status::ok) sources_.set_vsync(enable != 0u);
    return NtStatus(status).value();
}

/* A null allocation leaves the source showing nothing. */
/* Every flip, FlipOnNextVSync or FlipImmediate, shows from the next scanout:
 * VidSch completes an immediate flip on return and expects no report. A primary
 * in the implicit system memory segment arrives as segment 0, address 0
 * (VIDMM_SEGMENT::DriverId); VidSch matches the vsync against that address. */
int32_t Adapter::set_vidpn_source_address(const DxgkargSetVidPnSourceAddressRecord *request)
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::set_vidpn_source_address");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (request->hAllocation == nullptr)
        return NtStatus(sources_.set_scanout(request->VidPnSourceId, 0, nullptr)).value();
    if (request->PrimarySegment != ApertureSegment::segment_id && request->PrimarySegment != system_memory_driver_segment)
        return NtStatus(Status::invalid_parameter).value();
    log_scanout(*request);
    return NtStatus(sources_.set_scanout(request->VidPnSourceId, request->PrimaryAddress, request->hAllocation))
        .value();
}

/* Which source dxgkrnl showed or hid, and not only that it asked: a display the
 * power policy turned off comes back through this, and the log is the only way
 * to see whether it did. */
void Adapter::log_visibility(const DxgkargSetVidPnSourceVisibilityRecord &request, int32_t status)
{
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("rvkgpu: SetVidPnSourceVisibility source ");
    message.append_decimal(request.VidPnSourceId);
    message.append(request.Visible != 0u ? " visible" : " hidden");
    message.append(" status 0x");
    message.append_hex(static_cast<uint32_t>(status), 8u);
    platform_.log(message.text());
}

void Adapter::log_scanout(const DxgkargSetVidPnSourceAddressRecord &request)
{
    if (platform_.current_irql() >= Irql::dispatch || !budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("rvkgpu: SetVidPnSourceAddress source ");
    message.append_decimal(request.VidPnSourceId);
    message.append(" segment ");
    message.append_decimal(request.PrimarySegment);
    message.append(" address 0x");
    message.append_hex(static_cast<uint64_t>(request.PrimaryAddress), 16u);
    platform_.log(message.text());
}

int32_t Adapter::set_vidpn_source_visibility(const DxgkargSetVidPnSourceVisibilityRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_vidpn_source_visibility");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t set = NtStatus(sources_.set_visible(request->VidPnSourceId, request->Visible != 0u)).value();
    log_visibility(*request, set);
    return set;
}

int32_t Adapter::present(void *context, DxgkargPresentRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::present");
    if (request == nullptr || contexts_.find(context) == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t presented = PresentRequest(platform_, *request).present().value();
    log_status("rvkgpu: Present", presented);
    return presented;
}

int32_t Adapter::set_pointer_shape(const DxgkargSetPointerShapeRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_pointer_shape");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(cursor_.set_shape(*request)).value();
}

int32_t Adapter::set_pointer_position(const DxgkargSetPointerPositionRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_pointer_position");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(cursor_.set_position(*request)).value();
}

int32_t Adapter::acquire_swizzling_range(void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::acquire_swizzling_range");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::release_swizzling_range(const void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::release_swizzling_range");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::set_palette(const void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_palette");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::stop_capture(const void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_capture");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::create_overlay(void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_overlay");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::query_vidpn_hw_capability(DxgkargQueryVidPnHwCapabilityRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_vidpn_hw_capability");
    if (request == nullptr || request->hFunctionalVidPn == nullptr ||
        request->SourceId >= sources_.count() || request->TargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    request->VidPnHWCaps = 0u;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_dependent_engine_group(DxgkargQueryDependentEngineGroupRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_dependent_engine_group");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    request->DependentNodeOrdinalMask = uint64_t{1} << request->NodeOrdinal;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_engine_status(DxgkargQueryEngineStatusRecord *request)
{
    /* VidSch asks from its timeout DPC: DdiQueryEngineStatus (dxgkrnl.sys
     * @140027dc0) only reaches for the current thread when IRQL is below 2, so
     * dispatch is a level it expects to be called at. */
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::query_engine_status");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    /* VidSchiCheckNodeTimeout takes responsive as "no timeout this round". */
    if (progress_.responsive()) request->EngineStatus |= engine_responsive;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::reset_engine(DxgkargResetEngineRecord *request)
{
    /* The same DPC resets what it found stuck; DdiResetEngine (dxgkrnl.sys
     * @14006e0a4) checks only that the driver returns at the IRQL it entered. */
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::reset_engine");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    /* VidSch already knows what it reset, so nothing more is reported; every
     * other context keeps running - a reset of one engine is not an outage
     * (FR-002, FR-003). */
    request->LastAbortedFenceId = node_.abort_pending();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::cancel_command(const DxgkargCancelCommandRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::cancel_command");
    if (request == nullptr || contexts_.find(request->hContext) == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::notify_surprise_removal(int32_t removal_type)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::notify_surprise_removal");
    if (removal_type != removal_hibernation && removal_type != removal_pnp_notify)
        return NtStatus(Status::invalid_parameter).value();
    present_ = false;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::stop_device_and_release_post_display_ownership(uint32_t target, DxgkDisplayInformationRecord *display)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_device_and_release_post_display_ownership");
    if (display == nullptr || target >= sources_.count()) return NtStatus(Status::invalid_parameter).value();
    *display = DxgkDisplayInformationRecord{};
    return stop_device();
}

int32_t Adapter::get_child_container_id(uint32_t child, DxgkChildContainerIdRecord *container)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_child_container_id");
    if (container == nullptr || child >= sources_.count()) return NtStatus(Status::invalid_parameter).value();
    edid_.describe(*container);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::system_display_enable(uint32_t, void *, uint32_t *, uint32_t *, int32_t *)
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::system_display_enable");
    return NtStatus(Status::not_supported).value();
}

void Adapter::system_display_write(const void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::system_display_write");
}

int32_t Adapter::set_power_component_fstate(uint32_t, uint32_t)
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::set_power_component_fstate");
    return NtStatus(Status::invalid_parameter).value();
}

int32_t Adapter::power_runtime_control_request(const void *, const void *, size_t, void *, size_t, size_t *)
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::power_runtime_control_request");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::set_vidpn_source_address_with_multi_plane_overlay(const void *)
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::set_vidpn_source_address_with_multi_plane_overlay");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::get_node_metadata(uint32_t node_and_adapter, DxgkNodeMetadataRecord *metadata)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_node_metadata");
    if (metadata == nullptr || node_and_adapter != 0u) return NtStatus(Status::invalid_parameter).value();
    static constexpr char name[] = "3DNode00";
    *metadata = DxgkNodeMetadataRecord{};
    metadata->EngineType = engine_type_3d;
    metadata->GpuMmuSupported = 1u;
    for (uint32_t index = 0u; index + 1u < sizeof name; index++)
        metadata->FriendlyName[index] = static_cast<uint16_t>(name[index]);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::set_power_pstate(uint32_t, uint32_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_power_pstate");
    return NtStatus(Status::invalid_parameter).value();
}

int32_t Adapter::control_interrupt2(DxgkargControlInterrupt2Record control)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::control_interrupt2");
    if (control.InterruptType != interrupt_crtc_vsync) return NtStatus(Status::not_supported).value();
    if (control.CrtcVsyncState < vsync_enable || control.CrtcVsyncState > vsync_disable_no_phase)
        return NtStatus(Status::invalid_parameter).value();
    return control_interrupt(control.InterruptType, control.CrtcVsyncState == vsync_enable ? 1u : 0u);
}

int32_t Adapter::calibrate_gpu_clock(uint32_t node, uint32_t engine, DxgkGpuClockDataRecord *clock)
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::calibrate_gpu_clock");
    if (clock == nullptr || node != 0u || engine != 0u) return NtStatus(Status::invalid_parameter).value();
    uint64_t frequency = 0u;
    const uint64_t counter = platform_.performance_counter(frequency);
    clock->GpuFrequency = frequency;
    clock->GpuClockCounter = counter;
    clock->CpuClockCounter = counter;
    clock->Flags = 0u;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::submit_command_virtual(const DxgkargSubmitCommandVirtualRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::submit_command_virtual");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    const Context *context = contexts_.find(request->hContext);
    if (context == nullptr || !context->admits(request->DmaBufferVirtualAddress))
        return NtStatus(Status::invalid_parameter).value();
    /* The device that would have run this is not the one this adapter started
     * on, so no completion would ever come; SubmitCommandVirtual may refuse,
     * and D3DKMTSubmitCommand fails for the process instead (FR-005). */
    if (identity_.changed()) return NtStatus(Status::invalid_parameter).value();
    const Status status = context->is_rvk() ? submit_rvk(*request) : node_.submit(request->SubmissionFenceId);
    if (status == Status::ok) {
        static_cast<void>(dxgkrnl_.queue_dpc());
        return NtStatus(Status::ok).value();
    }
    /* A refusal is what dxgkrnl answers the process with, and may be what puts
     * its device in error: the log is the only place the reason survives. */
    log_status("rvkgpu: SubmitCommandVirtual refused", NtStatus(status).value());
    return NtStatus(Status::invalid_parameter).value();
}

/* The host may finish the batch before its packet is queued; the DPC queued
 * after both serves what the fence page already says, so nothing waits for an
 * interrupt that came first. */
Status Adapter::submit_rvk(const DxgkargSubmitCommandVirtualRecord &request)
{
    uint64_t             awaited = 0u;
    const Status published = rvk_contexts_.submit(request.hContext,
                                                  static_cast<const uint8_t *>(request.pDmaBufferPrivateData),
                                                  request.DmaBufferPrivateDataSize, awaited);
    if (published != Status::ok) return published;
    return node_.submit(request.SubmissionFenceId, request.hContext, awaited);
}

void Adapter::set_root_page_table(const DxgkargSetRootPageTableRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_root_page_table");
    Context *context = request != nullptr ? contexts_.find(request->hContext) : nullptr;
    if (context == nullptr || context->assign_root(*request) != Status::ok)
        platform_.log("rvkgpu: SetRootPageTable named no context or no usable root");
}

size_t Adapter::get_root_page_table_size(DxgkargGetRootPageTableSizeRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_root_page_table_size");
    return request != nullptr ? static_cast<size_t>(geometry_.root_table_bytes(request->NumberOfPte)) : 0u;
}

int32_t Adapter::create_process(DxgkargCreateProcessRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_process");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    request->hKmdProcess = nullptr;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::destroy_process(void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_process");
    return NtStatus(Status::ok).value();
}

void Adapter::set_stable_power_state(const DxgkargSetStablePowerStateRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_stable_power_state");
}

int32_t Adapter::control_mode_behavior(DxgkargControlModeBehaviorRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::control_mode_behavior");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    capabilities_.answer_mode_behavior(*request);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::update_monitor_link_info(DxgkargUpdateMonitorLinkInfoRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::update_monitor_link_info");
    if (request == nullptr || request->VideoPresentTargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    edid_.describe_link(*request);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::create_hw_queue(void *context, DxgkargCreateHwQueueRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_hw_queue");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(hw_queues_.create(context, *request)).value();
}

int32_t Adapter::destroy_hw_queue(void *queue)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_hw_queue");
    hw_submissions_.forget(queue);
    return NtStatus(hw_queues_.destroy(queue)).value();
}

int32_t Adapter::submit_command_to_hw_queue(const DxgkargSubmitCommandToHwQueueRecord *request)
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::submit_command_to_hw_queue");
    if (request == nullptr || request->DmaBufferPrivateDataSize != 0u) return NtStatus(Status::invalid_parameter).value();
    const Status status = hw_submissions_.submit(request->hHwQueue, request->HwQueueProgressFenceId,
                                                 static_cast<uint64_t *>(request->HwQueueProgressFenceCpuVa));
    if (status == Status::ok) static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(status).value();
}

int32_t Adapter::reset_hw_engine(const DxgkargResetHwEngineRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::reset_hw_engine");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    hw_submissions_.abandon();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::resume_hw_engine(const DxgkargResumeHwEngineRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::resume_hw_engine");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    return NtStatus(Status::ok).value();
}

bool Adapter::invariants_hold() const
{
    const bool powered_only_when_started = !device_.constructed() || started_;
    const bool mapped_only_when_started = mappings_.mapped_bars() == 0u || started_;
    return powered_only_when_started && mapped_only_when_started;
}

}  // namespace av
