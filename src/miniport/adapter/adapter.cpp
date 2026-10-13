#include "miniport/adapter/adapter.hpp"

#include "miniport/adapter/debug_report.hpp"
#include "miniport/adapter/trace_escape.hpp"
#include "miniport/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "miniport/ddi/dxgk_child_container_id_record.hpp"
#include "miniport/ddi/dxgk_child_descriptor_record.hpp"
#include "miniport/ddi/dxgk_child_status_record.hpp"
#include "miniport/ddi/dxgk_device_descriptor_record.hpp"
#include "miniport/ddi/dxgk_device_info_record.hpp"
#include "miniport/ddi/dxgk_display_information_record.hpp"
#include "miniport/ddi/dxgk_gpu_clock_data_record.hpp"
#include "miniport/ddi/dxgk_node_metadata_record.hpp"
#include "miniport/ddi/dxgkargcb_query_feature_support_record.hpp"
#include "miniport/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "miniport/ddi/dxgkarg_cancel_command_record.hpp"
#include "miniport/ddi/dxgkarg_close_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "miniport/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_create_context_record.hpp"
#include "miniport/ddi/dxgkarg_create_device_record.hpp"
#include "miniport/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/escape/umd_adapter_info.hpp"
#include "proto/escape/follow_record.hpp"
#include "proto/resources/surface_point.hpp"
#include "proto/escape/present_point_record.hpp"
#include "miniport/present/present_ticket_record.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/escape/wake_event_record.hpp"
#include "proto/escape/tail_publish_record.hpp"
#include "miniport/ddi/dxgkarg_native_fence_records.hpp"
#include "miniport/ddi/dxgkddi_feature_interface_record.hpp"
#include "miniport/ddi/entry_points.hpp"
#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "miniport/scheduling/hw_queue.hpp"
#include "miniport/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "miniport/ddi/dxgkarg_create_process_record.hpp"
#include "miniport/ddi/dxgkarg_describe_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_escape_record.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "proto/escape/host_window_record.hpp"
#include "proto/escape/trace_escape_record.hpp"
#include "miniport/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "miniport/ddi/dxgkarg_get_scan_line_record.hpp"
#include "miniport/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "miniport/ddi/dxgkarg_open_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_patch_record.hpp"
#include "miniport/ddi/dxgkarg_preempt_command_record.hpp"
#include "miniport/ddi/dxgkarg_present_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/ddi/dxgkarg_query_current_fence_record.hpp"
#include "miniport/ddi/dxgkarg_query_dependent_engine_group_record.hpp"
#include "miniport/ddi/dxgkarg_query_engine_status_record.hpp"
#include "miniport/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
#include "miniport/ddi/dxgkarg_render_record.hpp"
#include "miniport/ddi/dxgkarg_reset_engine_record.hpp"
#include "miniport/ddi/dxgkarg_reset_hw_engine_record.hpp"
#include "miniport/ddi/dxgkarg_resume_hw_engine_record.hpp"
#include "miniport/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "miniport/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "miniport/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "miniport/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "miniport/ddi/dxgkarg_set_stable_power_state_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_address_record.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_address_with_multi_plane_overlay_record.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_address_with_multi_plane_overlay3_record.hpp"
#include "miniport/ddi/dxgkarg_check_multiplane_overlay_support3_record.hpp"
#include "miniport/ddi/dxgkarg_multiplane_overlay_caps_records.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_visibility_record.hpp"
#include "miniport/ddi/dxgkarg_submit_command_record.hpp"
#include "miniport/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "miniport/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "miniport/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "miniport/ddi/linked_device_record.hpp"
#include "miniport/display/monitor_modes.hpp"
#include "miniport/memory/paging_request.hpp"
#include "miniport/memory/standard_allocation.hpp"
#include "miniport/driver/adapter_info_query.hpp"
#include "miniport/pci/pci_base_addresses.hpp"
#include "miniport/memory/device_allocation.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/present/present_blit.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/dma/rectangle.hpp"
#include "miniport/present/present_request.hpp"
#include "miniport/present/present_ticket.hpp"
#include "miniport/present/presented_allocations.hpp"
#include "miniport/adapter/capset_exchange.hpp"
#include "miniport/scheduling/patch_request.hpp"
#include "miniport/scheduling/render_request.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/text_buffer.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"
#include "core/support/scope_exit.hpp"
#include "miniport/vidpn/cofunc_modality.hpp"
#include "miniport/vidpn/path_transformation.hpp"
#include "miniport/vidpn/vidpn_commit.hpp"
#include "miniport/vidpn/vidpn_support_check.hpp"
#include "miniport/virtio/virtio_capability_walk.hpp"
#include "miniport/virtio/virtio_region.hpp"

namespace av {

namespace {

/* STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER: a present asking for more room, not a refusal. */
constexpr int32_t more_dma_buffer = static_cast<int32_t>(0xC01E0001u);

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
      resources_(platform), mappings_(platform, dxgkrnl_), layout_(), device_(), mapped_pixels_(platform), copier_(), presents_(), present_epochs_(0u), doorbells_(), doorbell_table_(), capabilities_(platform), geometry_(platform),
      segment_(platform), host_visible_(platform), memory_(platform, segment_), window_(platform),
      display_info_(), modes_(), edid_(platform, modes_.preferred()), sources_(platform), overlays_(platform, sources_),
      shared_(platform), allocations_(platform, segment_, memory_, shared_),
      devices_(platform, allocations_, shared_, dxgkrnl_), contexts_(platform, devices_, geometry_),
      av_contexts_(platform, dxgkrnl_, allocations_, window_),
      hw_queues_(platform, contexts_), hw_submissions_(platform), hw_queue_contexts_(platform),
      native_fences_(platform), hardware_scheduling_(false), user_mode_submission_(false), node_(platform), progress_(platform, node_), transition_(), identity_(platform), description_(platform), notification_(platform, dxgkrnl_), dpc_lock_(platform, LockRank::dpc), vsync_(platform, dxgkrnl_, sources_),
      cursor_(platform, sources_, device_), scanout_(platform, budget_, device_), presented_fence_(0u), presents_made_(0u), presents_shown_(0u), refreshes_owed_(0u), refreshed_at_(0u), refresh_waiting_(false), present_rows_(0u), presents_unmoved_(0u), presents_unticketed_(0u),
      display_changed_(0u), display_announced_(0u), started_(false), luid_(0u), present_(true)
      {
          AV_SCOPE("Adapter::Adapter");
      }

Adapter::~Adapter()
{
    AV_SCOPE("Adapter::~Adapter");
    static_cast<void>(stop_device());
}

void *Adapter::physical_device_object() const
{
    AV_SCOPE("Adapter::physical_device_object");
    platform_.require_irql_at_most(Irql::isr, "Adapter::physical_device_object");
    return physical_device_object_;
}

Status Adapter::discover_and_map(const DxgkrnlInterfaceRecord *dxgkrnl_interface)
{
    AV_SCOPE("Adapter::discover_and_map");
    Status status = dxgkrnl_.initialize(dxgkrnl_interface);
    /* What this OS allows before anything is reported: the queue DDIs are
     * implemented and tested, and whether a node may say so is the OS's to
     * answer (research.md §15). */
    if (status == Status::ok) {
        hardware_scheduling_ = dxgkrnl_.feature_enabled(feature_hwsch, feature_support_stable);
        /* The capability is reportable only where the OS allowed the feature:
         * claiming it otherwise fails the adapter start (research.md §17). */
        if (dxgkrnl_.feature_enabled(feature_native_fence, feature_support_stable)) {
            capabilities_.native_fences_allowed();
            log_status("avgpu: native fences", 1);
        } else {
            log_status("avgpu: native fences", 0);
        }
        /* And only where the fence half of WDDM 3.1 is live beside it: a node
         * that reports user-mode submission without the NativeGpuFence
         * capability fails the adapter start (research.md §14), and this OS
         * build refuses the native fence feature outright (§20). */
        user_mode_submission_ = hardware_scheduling_ && capabilities_.native_fences_reportable() &&
                                dxgkrnl_.feature_enabled(feature_user_mode_submission, feature_support_stable);
        log_status("avgpu: hardware scheduling", hardware_scheduling_ ? 1 : 0);
        log_status("avgpu: user-mode submission", user_mode_submission_ ? 1 : 0);
    }
    DxgkDeviceInfoRecord info = {};
    if (status == Status::ok) status = dxgkrnl_.get_device_information(info);
    if (status == Status::ok) status = segment_.initialize(info.SystemMemorySize);
    /* The number this adapter stamps into every shared object it mints, so a
     * record another adapter of this kernel wrote is one it refuses (053). A
     * start that follows a stop keeps the one it already has. */
    if (status == Status::ok && shared_.adapter() == 0u) status = shared_.initialize();
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
    AV_SCOPE("Adapter::map_doorbells");
    const VirtioRegion region = layout_.shared_memory(doorbell_region);
    if (!region.present()) return Status::ok;
    const Status mapped = mappings_.map_region(bars, resources_, region);
    if (mapped != Status::ok) return mapped;
    /* Neither storage can already hold one: a start runs only on a stopped
     * adapter, and both are destroyed on the way out of a failed start and on
     * stop, so the only way construct answers anything else is unreachable. */
    static_cast<void>(doorbells_.construct(platform_, mappings_.address_of(region), region.length()));
    return doorbell_table_.construct(platform_, doorbells_.object());
}

void Adapter::remember_device()
{
    AV_SCOPE("Adapter::remember_device");
    CapsetInfo     capset;
    CapsetExchange exchange(platform_, device_.object());
    const Status   read = exchange.read(capset);
    if (read == Status::ok) {
        identity_.remember(capset);
        start_presents();
        host_visible_.offer(capset);
        memory_.offer_local(capset);
        scanout_.offer(capset);
        overlays_.offer(capset);
        /* A window this driver cannot place chunks in is logged, never a
         * failed start: every chunk then falls back to guest RAM. */
        if (host_visible_.offered())
            log_status("avgpu: host window",
                       NtStatus(window_.initialize(host_visible_.base(), host_visible_.size())).value());
        /* A name that did not reach the registry is the INF's name standing,
         * which is a working adapter: logged, never a failed start. */
        log_status("avgpu: adapter description",
                   NtStatus(description_.publish(physical_device_object_, capset.gpu())).value());
    }
    log_status("avgpu: device capset", NtStatus(read).value());
}

/* Chunks already placed stay recorded, and are given back into the window as
 * their contexts close them. */
void Adapter::withdraw_window()
{
    AV_SCOPE("Adapter::withdraw_window");
    window_.withdraw();
    host_visible_.forget();
}

void Adapter::judge_device()
{
    AV_SCOPE("Adapter::judge_device");
    if (!identity_.known()) return;
    CapsetInfo     capset;
    CapsetExchange exchange(platform_, device_.object());
    if (exchange.read(capset) != Status::ok) {
        withdraw_window();
        return;
    }
    /* The same device, built again: its window is empty, and the contexts the
     * reset abandoned let go of their chunks as they are destroyed. */
    if (identity_.accept(capset)) return;
    /* Nothing runs on a device that changed, and its region is not the one
     * VidMm was told about. What an av context queued for it stays on the
     * node and never completes: the engine stops moving, and dxgkrnl's timeout
     * detection resets it through ResetEngine. Dropping it here would leave an
     * idle engine answering responsive while VidSch still waits on it, and
     * telling dxgkrnl would mean DMA_FAULTED, which bugchecks this adapter
     * (docs/kb/05-scheduling.md). The display is the scanout the transition
     * restored, none of this, and keeps working. */
    withdraw_window();
    av_contexts_.abandon_all();
}

Status Adapter::power_up()
{
    AV_SCOPE("Adapter::power_up");
    /* The only refusal is storage already holding a device: already powered. */
    if (device_.construct(platform_, mappings_, layout_) != Status::ok) return Status::ok;
    const Status status = device_.object().start(resources_.message_interrupts());
    if (status != Status::ok) device_.destroy();
    if (status == Status::ok) start_copier();
    if (status == Status::ok) start_presents();
    if (status == Status::ok) attach_clock();
    cursor_.forget_device();
    scanout_.forget_device();
    return status;
}

/* A removed device cannot acknowledge a reset; destruction still abandons it. */
Status Adapter::power_down()
{
    AV_SCOPE("Adapter::power_down");
    if (!device_.constructed()) return Status::ok;
    /* A reset destroys every host context and blob, and the device object goes
     * with it. The window stays offered for whatever brings the device back. */
    av_contexts_.abandon_all();
    presents_.destroy();
    copier_.destroy();
    const Status status = present_ ? device_.object().stop() : Status::ok;
    device_.destroy();
    return status;
}

/* A device that will not keep the display's clock leaves dxgkrnl with no
 * vertical blank from it, which stalls every flip; the start carries on, and
 * the line is the only trace of why. */
void Adapter::attach_clock()
{
    AV_SCOPE("Adapter::attach_clock");
    const Status attached = vsync_.attach(device_.object());
    if (attached == Status::ok) return;
    char       bytes[80];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: the device refused the display clock, status 0x");
    message.append_hex(static_cast<uint32_t>(NtStatus(attached).value()), 8u);
    platform_.log(message.text());
}

/* A device that does not offer the engine, or will not take its page, has
 * its presents moved before DdiPresent returns, as before the engine; the
 * line says which. The capset is the one StartDevice read: a power-up before
 * it knows nothing yet, and StartDevice starts the engine once it does. */
void Adapter::start_presents()
{
    AV_SCOPE("Adapter::start_presents");
    if (!device_.constructed() || presents_.constructed() || !identity_.offers(CapsetInfo::feature_present_engine))
        return;
    present_epochs_++;
    if (presents_.construct(platform_, device_.object(), present_epochs_) != Status::ok) return;
    const Status started = presents_.object().start();
    if (started == Status::ok) {
        platform_.log("avgpu: presents are moved by the device's present engine");
        return;
    }
    presents_.destroy();
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: the device's present engine was not started, status 0x");
    message.append_hex(static_cast<uint32_t>(NtStatus(started).value()), 8u);
    platform_.log(message.text());
}

/* Without contiguous memory for its payload the host cannot be handed a
 * present, and the miniport moves every one itself. */
void Adapter::start_copier()
{
    AV_SCOPE("Adapter::start_copier");
    if (copier_.construct(platform_, device_.object(), mapped_pixels_) != Status::ok) return;
    if (copier_.object().initialize() == Status::ok) return;
    copier_.destroy();
    platform_.log("avgpu: no memory to hand presents to the host; the miniport moves their pixels");
}

int32_t Adapter::start_device(void *start_info, const DxgkrnlInterfaceRecord *dxgkrnl_interface, uint32_t *sources,
                              uint32_t *children)
{
    AV_SCOPE("Adapter::start_device");
    platform_.require_irql_at_most(Irql::passive, "Adapter::start_device");
    InvariantScope<Adapter> guard(platform_, *this, "start_device");
    if (started_) return NtStatus(Status::invalid_state).value();
    constexpr size_t start_info_luid_offset = 20u;   /* after RequiredDmaQueueEntry and AdapterGuid */
    if (start_info != nullptr) {
        uint32_t luid[2] = {};
        __builtin_memcpy(luid, static_cast<const uint8_t *>(start_info) + start_info_luid_offset, sizeof(luid));
        luid_ = static_cast<uint64_t>(luid[1]) << 32u | luid[0];
    }
    if (dxgkrnl_interface == nullptr || sources == nullptr || children == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    present_ = true;
    ScopeExit undo([this] {
        cursor_.release();
        scanout_.release();
        vsync_.destroy();
        doorbell_table_.destroy();
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
        message.append("avgpu: StartDevice failed, status 0x");
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
    log_status("avgpu: display info", NtStatus(follow_display_info()).value());
    remember_device();
    *sources = sources_.count();
    *children = sources_.count();
    started_ = true;
    platform_.log("avgpu: StartDevice");
    return NtStatus(Status::ok).value();
}

int32_t Adapter::stop_device()
{
    AV_SCOPE("Adapter::stop_device");
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_device");
    InvariantScope<Adapter> guard(platform_, *this, "stop_device");
    /* The device first: its clock writes the page until it is reset. */
    const Status status = power_down();
    vsync_.destroy();
    scanout_.release();
    cursor_.release();
    doorbell_table_.destroy();
    doorbells_.destroy();
    mappings_.unmap_all();
    identity_.forget();
    memory_.forget_local();
    withdraw_window();
    started_ = false;
    const int32_t stopped = NtStatus(status).value();
    log_status("avgpu: StopDevice", stopped);
    return stopped;
}

int32_t Adapter::dispatch_io_request(uint32_t, void *)
{
    AV_SCOPE("Adapter::dispatch_io_request");
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
    AV_SCOPE("Adapter::interrupt_routine");
    platform_.require_irql_at_most(Irql::isr, "Adapter::interrupt_routine");
    /* A device that is being built is not one to read from: power_up
     * constructs it and then starts it, and dxgkrnl leaves the interrupt
     * connected across the whole transition. */
    if (!device_.constructed() || !present_ || !device_.object().running()) return 0u;
    const bool    messages = resources_.message_interrupts() != 0u;
    const uint8_t isr = messages ? 0u : device_.object().read_isr();
    if (!messages && isr == 0u) return 0u;
    if (messages ? message == config_message : (isr & isr_configuration_changed) != 0u) {
        platform_.store_atomic(display_changed_, 1u);
        platform_.store_atomic(display_announced_, 1u);
    }
    static_cast<void>(dxgkrnl_.queue_dpc());
    return 1u;
}

void Adapter::dpc_routine()
{
    AV_SCOPE("Adapter::dpc_routine");
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::dpc_routine");
    SpinLockGuard  serialized(dpc_lock_);
    const uint64_t now = platform_.interrupt_time();
    uint32_t       completed = 0u;
    const bool     harvest = device_.constructed() && present_ && device_.object().running();
    /* The blank first, and dxgkrnl told before the scanout is refreshed: a
     * refresh kicks the device, and a kick QEMU handles on the vCPU waits for
     * its main loop - a vsync reported behind one reached dxgkrnl milliseconds
     * after the blank. */
    if (harvest) vsync_.tick(now, scanout_.posts_frames(vsync_source));
    announce_display_change(harvest);
    /* A device that fails every harvest would otherwise log on every DPC, so
     * each of these lines is spent from the budget (feature 037, FR-003). */
    if (harvest && device_.object().harvest_control(completed) != Status::ok && budget_.allows())
        platform_.log("avgpu: control queue harvest failed in the DPC");
    if (harvest && device_.object().harvest_cursor(completed) != Status::ok && budget_.allows())
        platform_.log("avgpu: cursor queue harvest failed in the DPC");
    av_contexts_.serve(node_, hw_submissions_);
    if (harvest && presents_.constructed()) node_.finish(&presents_.object(), presents_.object().completed());
    const uint32_t overdue = node_.release_overdue(now);
    if (overdue != 0u && budget_.allows()) platform_.log("avgpu: a held packet passed its deadline and was let go");
    if (node_.complete(notification_) != Status::ok && budget_.allows())
        platform_.log("avgpu: a completion was refused in the DPC");
    if (hw_submissions_.complete(notification_) != Status::ok && budget_.allows())
        platform_.log("avgpu: a progress fence signal was refused in the DPC");
    /* A frame is carried when a submission completed since the last DPC, never
     * because the control queue answered: our own scanout requests complete
     * there, and taking those for presents would flush forever. */
    constexpr uint32_t refreshes_per_present = 2u;
    const uint32_t     fence = node_.current_fence();
    const bool         completed_any = fence != presented_fence_;
    presented_fence_ = fence;
    const uint64_t made = platform_.load_atomic(presents_made_);
    if (made != presents_shown_) {
        presents_shown_ = made;
        refreshes_owed_ = refreshes_per_present;
    }
    const bool presented = completed_any && refreshes_owed_ != 0u;
    if (presented) refreshes_owed_--;
    refresh_waiting_ = refresh_waiting_ || presented;
    dxgkrnl_.notify_dpc();
    if (harvest) refresh_scanout(now);
}

/* A primary the host reads back is carried once a display period at most: an
 * application flipping thousands of times a second would otherwise have the
 * host read every frame. A numbered flip goes on every DPC, since the device's
 * clock shows only what it has before its blank. */
void Adapter::refresh_scanout(uint64_t now)
{
    AV_SCOPE("Adapter::refresh_scanout");
    constexpr uint64_t refresh_period = 10000000u / DisplayMode::refresh_hz - 10000u;   /* 100 ns, a ms early */
    const bool         due = now - refreshed_at_ >= refresh_period;
    const bool         shown = due && refresh_waiting_;
    if (due) refreshed_at_ = now;
    if (due) refresh_waiting_ = false;
    if (scanout_.refresh(sources_, shown, due) != Status::ok && budget_.allows())
        platform_.log("avgpu: a scanout refresh was refused in the DPC");
}

/* One video output per display the device has. dxgkrnl's array holds one
 * descriptor more than the children StartDevice reported, zeroed, so the buffer
 * is checked against that. */
int32_t Adapter::query_child_relations(DxgkChildDescriptorRecord *relations, uint32_t size)
{
    AV_SCOPE("Adapter::query_child_relations");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_relations");
    if (relations == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (size < (sources_.count() + 1u) * sizeof *relations) return NtStatus(NtStatus::buffer_too_small).value();
    platform_.log("avgpu: QueryChildRelations");
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
    AV_SCOPE("Adapter::query_child_status");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_status");
    if (status == nullptr || status->ChildUid >= sources_.count()) return NtStatus(Status::invalid_parameter).value();
    if (status->Type == status_rotation) return NtStatus(Status::invalid_parameter).value();
    int32_t answered = NtStatus(Status::not_supported).value();
    if (status->Type == status_connection) {
        /* A device with no display attached - a second host GPU, which
         * renders but drives nothing - enables none of its scanouts. */
        status->HotPlugConnected = display_info_.enabled(status->ChildUid) ? 1u : 0u;
        answered = NtStatus(Status::ok).value();
    }
    log_status("avgpu: QueryChildStatus", answered);
    return answered;
}

int32_t Adapter::query_device_descriptor(uint32_t child, DxgkDeviceDescriptorRecord *descriptor)
{
    AV_SCOPE("Adapter::query_device_descriptor");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_device_descriptor");
    if (child >= sources_.count() || descriptor == nullptr) return NtStatus(Status::invalid_parameter).value();
    follow_display_change();
    /* The monitor's mode set is built from this EDID, so a bring-up that never
     * reaches a committable VidPn needs the answer visible. */
    const int32_t copied = edid_.copy(*descriptor).value();
    log_status("avgpu: QueryDeviceDescriptor", copied);
    return copied;
}

int32_t Adapter::set_power_state(uint32_t device, int32_t power_state, int32_t)
{
    AV_SCOPE("Adapter::set_power_state");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_power_state");
    InvariantScope<Adapter> guard(platform_, *this, "set_power_state");
    if (device != adapter_device_uid) return NtStatus(Status::ok).value();
    if (!started_) return NtStatus(Status::invalid_state).value();
    if (power_state == power_d0) {
        platform_.log("avgpu: SetPowerState D0");
        const Status powered = power_up();
        log_status("avgpu: SetPowerState D0 device", NtStatus(powered).value());
        /* What power took away, put back: dxgkrnl kept every allocation and
         * VidPn across this, so what it expects to find is what was scanning
         * out before. */
        if (powered == Status::ok && transition_.held()) restore_scanouts();
        if (powered == Status::ok)
            log_status("avgpu: SetPowerState D0 vsync", NtStatus(vsync_.resume()).value());
        if (powered == Status::ok) judge_device();
        transition_.release();
        platform_.log("avgpu: SetPowerState D0 done");
        return NtStatus(powered).value();
    }
    if (power_state > power_d0 && power_state <= power_d3) {
        platform_.log("avgpu: SetPowerState D3");
        static_cast<void>(transition_.hold(device_.constructed(), scanning_out()));
        /* Before the device goes: a vertical blank reported to an adapter that
         * is not powered - or to one on its way out of S3, before dxgkrnl has
         * powered it up - is what bugchecks the guest. */
        vsync_.suspend();
        const int32_t down = NtStatus(power_down()).value();
        log_status("avgpu: SetPowerState D3 done", down);
        return down;
    }
    return NtStatus(Status::invalid_parameter).value();
}

/* dxgkrnl forwards ACPI events it does not handle itself; nothing to do. */
int32_t Adapter::notify_acpi_event(int32_t, uint32_t, void *, uint32_t *acpi_flags)
{
    AV_SCOPE("Adapter::notify_acpi_event");
    platform_.require_irql_at_most(Irql::passive, "Adapter::notify_acpi_event");
    *acpi_flags = 0u;
    return NtStatus(Status::ok).value();
}

void Adapter::reset_device()
{
    AV_SCOPE("Adapter::reset_device");
    platform_.require_irql_at_most(Irql::isr, "Adapter::reset_device");
    if (device_.constructed()) device_.object().reset_immediately();
}

/* GUID_WDDM_INTERFACE_FEATURE (dispmprt.h:451), the one interface this driver
 * answers: dxgkrnl asks for it to find out which staged features the driver
 * implements. */
namespace {
constexpr uint8_t feature_interface_guid[16] = {0x93, 0x39, 0xbb, 0x94, 0xc3, 0xc6, 0xa7, 0x4d,
                                                0x89, 0x49, 0xa1, 0x13, 0x82, 0x32, 0xe7, 0x59};
}  // namespace

int32_t Adapter::query_interface(void *query)
{
    AV_SCOPE("Adapter::query_interface");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_interface");
    if (query == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(answer_feature_interface(*static_cast<QueryInterfaceRecord *>(query))).value();
}

Status Adapter::answer_feature_interface(QueryInterfaceRecord &query)
{
    AV_SCOPE("Adapter::answer_feature_interface");
    if (query.InterfaceType == nullptr || query.Interface == nullptr) return Status::invalid_parameter;
    for (uint32_t byte = 0u; byte < sizeof feature_interface_guid; byte++)
        if (static_cast<const uint8_t *>(query.InterfaceType)[byte] != feature_interface_guid[byte])
            return Status::not_supported;
    if (query.Version != feature_interface_version_1 || query.Size < sizeof(DxgkddiFeatureInterfaceRecord))
        return Status::not_supported;
    DxgkddiFeatureInterfaceRecord &table = *static_cast<DxgkddiFeatureInterfaceRecord *>(query.Interface);
    table.Size = static_cast<uint16_t>(sizeof table);
    table.Version = feature_interface_version_1;
    table.Context = this;
    /* Nothing is reference counted: the table is this adapter's, and dxgkrnl
     * stops using it at RemoveDevice. */
    table.InterfaceReference = [](void *) {};
    table.InterfaceDereference = [](void *) {};
    table.QueryFeatureSupport = &AvDdiQueryFeatureSupport;
    table.QueryFeatureInterface = nullptr;
    return Status::ok;
}

/* The features this driver implements, and nothing else: a feature it does not
 * answer for stays disabled, which is what keeps a capability behind it
 * unreportable (research.md §18). */
int32_t Adapter::query_feature_support(DxgkargQueryFeatureSupportRecord *request)
{
    AV_SCOPE("Adapter::query_feature_support");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_feature_support");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    request->MinSupportedVersion = feature_version_lowest;
    request->MaxSupportedVersion = feature_version_highest;
    request->SupportedByDriver = 0u;
    request->SupportedOnCurrentConfig = 0u;
    const bool implemented = request->FeatureId == feature_native_fence ||
                             request->FeatureId == feature_user_mode_submission ||
                             request->FeatureId == feature_hwsch;
    if (!implemented) return NtStatus(Status::ok).value();
    request->SupportedByDriver = 1u;
    /* And on this configuration: dxgkrnl asks before StartDevice has mapped
     * anything, so what the driver answers is what it implements rather than
     * what it has mapped so far. A device that turns out to have no doorbell
     * pages hands out no queues and no doorbells, which is refused there
     * (Adapter::create_doorbell) rather than pretended here. */
    request->SupportedOnCurrentConfig = 1u;
    return NtStatus(Status::ok).value();
}

/* Present snapshots are offered while the engine runs on a device that reads
 * them: a user-mode driver that saw the bit stops reading blt presents back.
 * An integrated host GPU is said so, for the vendor libraries that describe
 * the GPU to applications. */
Status Adapter::answer_umd_private(const AdapterInfoQuery &request)
{
    AV_SCOPE("Adapter::answer_umd_private");
    const bool     snapshots = presents_.constructed() && identity_.offers(CapsetInfo::feature_present_from_image);
    const uint64_t features = (snapshots ? UmdAdapterInfo::feature_present_snapshots : 0u) |
                              (identity_.integrated() ? UmdAdapterInfo::feature_integrated_gpu : 0u);
    uint8_t        bytes[UmdAdapterInfo::encoded_size] = {};
    size_t         written = 0u;
    const uint32_t room = request.output_size();
    const Status   encoded = UmdAdapterInfo(luid_, features).encode(bytes, room < sizeof bytes ? room : sizeof bytes,
                                                                    written);
    if (encoded != Status::ok) return Status::invalid_parameter;
    return request.write_output(bytes, static_cast<uint32_t>(written));
}

int32_t Adapter::query_adapter_info(const DxgkargQueryAdapterInfoRecord *query)
{
    AV_SCOPE("Adapter::query_adapter_info");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_adapter_info");
    if (query == nullptr) return NtStatus(Status::invalid_parameter).value();
    const AdapterInfoQuery request(platform_, *query);
    int32_t                answered = NtStatus(NtStatus::not_implemented).value();
    switch (request.type()) {
    case query_umd_private:
        answered = NtStatus(answer_umd_private(request)).value();
        break;
    case query_driver_caps:
        answered = NtStatus(capabilities_.report_driver_caps(request)).value();
        break;
    case query_native_fence_caps:
        /* Answered only because the four native fence DDIs are implemented
         * (constitution VI) and the OS allowed the feature; where it did not,
         * this is a type this driver does not report at all (§17). */
        if (capabilities_.native_fences_reportable())
            answered = NtStatus(capabilities_.report_native_fence_caps(request)).value();
        break;
    case query_user_mode_submission_caps:
        /* Answered only because the doorbell DDIs are implemented (constitution
         * VI): dxgkrnl hands processes doorbell pages on the strength of it. */
        return NtStatus(capabilities_.report_user_mode_submission_caps(request)).value();
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
        message.append("avgpu: QueryAdapterInfo type ");
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
    AV_SCOPE("Adapter::escape");
    platform_.require_irql_at_most(Irql::passive, "Adapter::escape");
    if (escape == nullptr || escape->pPrivateDriverData == nullptr || escape->PrivateDriverDataSize == 0u)
        return NtStatus(Status::invalid_parameter).value();
    auto *const bytes = static_cast<uint8_t *>(escape->pPrivateDriverData);
    uint32_t    magic = 0u;
    if (escape->PrivateDriverDataSize >= sizeof magic) platform_.copy(&magic, bytes, sizeof magic);
    if (magic == TailPublishRecord::magic) return publish_own_tail(*escape);
    if (magic == HostWindowRecord::magic) return window_chunk(*escape);
    if (magic == ContextViewsRecord::magic) return context_views(*escape);
    if (magic == WakeEventRecord::magic) return wake_event(*escape);
    if (magic != TraceEscapeRecord::magic) return NtStatus(Status::not_supported).value();
    /* Only the adapter is named: the inspector opens no device. */
    const Status answered = TraceEscape(platform_, *this).answer(bytes, escape->PrivateDriverDataSize);
    return NtStatus(answered == Status::malformed ? Status::invalid_parameter : answered).value();
}

/* A process asking for a chunk on its av context - of the host window, or of
 * guest RAM where the window cannot hold it - or giving one back
 * (docs/host-window.md). A device that refuses the chunk's blob, or refuses to
 * let go of one an import still holds, is STATUS_NOT_SUPPORTED; every other
 * refusal is STATUS_INVALID_PARAMETER. */
int32_t Adapter::window_chunk(const DxgkargEscapeRecord &escape)
{
    AV_SCOPE("Adapter::window_chunk");
    auto *const            bytes = static_cast<uint8_t *>(escape.pPrivateDriverData);
    HostWindowRecord       record;
    VirtioGpuDevice *const device = answering_device();
    if (escape.hContext == nullptr || escape.PrivateDriverDataSize != HostWindowRecord::encoded_size ||
        record.decode(bytes, escape.PrivateDriverDataSize) != Status::ok || device == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    const Status status = av_contexts_.window_chunk(escape.hContext, record, *device);
    if (status == Status::not_supported) return NtStatus(status).value();
    if (status != Status::ok) return NtStatus(Status::invalid_parameter).value();
    static_cast<void>(record.encode(bytes, HostWindowRecord::encoded_size));   /* the length was checked */
    return NtStatus(Status::ok).value();
}

/* A process asking where its av context's ring and reply slot are: the driver
 * mapped them into it when the context was made (docs/protocol.md, "Context
 * memory"). Every refusal is STATUS_INVALID_PARAMETER. */
int32_t Adapter::context_views(const DxgkargEscapeRecord &escape)
{
    AV_SCOPE("Adapter::context_views");
    auto *const        bytes = static_cast<uint8_t *>(escape.pPrivateDriverData);
    ContextViewsRecord record;
    if (escape.hContext == nullptr || record.decode(bytes, escape.PrivateDriverDataSize) != Status::ok ||
        av_contexts_.views(escape.hContext, record) != Status::ok)
        return NtStatus(Status::invalid_parameter).value();
    static_cast<void>(record.encode(bytes, ContextViewsRecord::encoded_size));   /* decode checked the length */
    return NtStatus(Status::ok).value();
}

/* A process handing over an event to be set whenever its av context's host
 * raises (docs/protocol.md, "Wake event record"). A device that never moves
 * `raised` would never set it, and the process is told so rather than left to
 * sleep: STATUS_NOT_SUPPORTED, as from a driver without the record. Every
 * other refusal is STATUS_INVALID_PARAMETER. */
int32_t Adapter::wake_event(const DxgkargEscapeRecord &escape)
{
    AV_SCOPE("Adapter::wake_event");
    if (!identity_.offers(CapsetInfo::feature_raises)) return NtStatus(Status::not_supported).value();
    WakeEventRecord record;
    if (escape.hContext == nullptr ||
        record.decode(static_cast<const uint8_t *>(escape.pPrivateDriverData), escape.PrivateDriverDataSize) !=
            Status::ok ||
        av_contexts_.wake_on_raise(escape.hContext, record.event()) != Status::ok)
        return NtStatus(Status::invalid_parameter).value();
    return NtStatus(Status::ok).value();
}

/* A process that stores its own ring's tail before each submission: the
 * device may then take a batch before the scheduler hands this driver its
 * packet, which the context accepts as it does a doorbell's. */
int32_t Adapter::publish_own_tail(const DxgkargEscapeRecord &escape)
{
    AV_SCOPE("Adapter::publish_own_tail");
    TailPublishRecord    record;
    const Context *const context = escape.hContext != nullptr ? contexts_.find(escape.hContext) : nullptr;
    if (context == nullptr || !context->is_av() ||
        record.decode(static_cast<const uint8_t *>(escape.pPrivateDriverData), escape.PrivateDriverDataSize) !=
            Status::ok ||
        record.ring() != context->av_record().ring())
        return NtStatus(Status::invalid_parameter).value();
    return NtStatus(av_contexts_.user_mode_submits(escape.hContext, nullptr)).value();
}

int32_t Adapter::collect_dbg_info(const DxgkargCollectDbgInfoRecord *collect)
{
    AV_SCOPE("Adapter::collect_dbg_info");
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
    AV_SCOPE("Adapter::create_device");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_device");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const void   *runtime = request->hDevice;
    const int32_t created = NtStatus(devices_.create(*this, *request)).value();
    log_handle("avgpu: CreateDevice runtime", runtime, created);
    return created;
}

int32_t Adapter::destroy_device(void *device)
{
    AV_SCOPE("Adapter::destroy_device");
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_device");
    return NtStatus(devices_.destroy(device)).value();
}

int32_t Adapter::create_allocation(DxgkargCreateAllocationRecord *request)
{
    AV_SCOPE("Adapter::create_allocation");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t created = NtStatus(allocations_.create(*request)).value();
    log_allocation(*request, created);
    return created;
}

/* A source showing one of these must stop before the allocation goes: the DPC
 * follows the handle to find the primary's pages. */
int32_t Adapter::destroy_allocation(const DxgkargDestroyAllocationRecord *request)
{
    AV_SCOPE("Adapter::destroy_allocation");
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (request->pAllocationList != nullptr)
        for (uint32_t index = 0u; index < request->NumAllocations; index++) {
            sources_.forget_primary(request->pAllocationList[index]);
            /* A context is named by its ring allocation, and another context
             * of the process shares with it by that name (feature 050); a name
             * freed while the context lives could be a new allocation's, so the
             * context goes with it - which is the order a process exit uses. */
            void *const user = av_contexts_.user_of(allocations_.find(request->pAllocationList[index]));
            if (user != nullptr) static_cast<void>(av_contexts_.unbind(user, answering_device()));
        }
    const Status destroyed = allocations_.destroy(*request);
    release_shared();
    return NtStatus(destroyed).value();
}

/* dxgkrnl destroys a shared allocation only once every process let go of it,
 * so what the destroy forgot is gone from the whole guest: the device holds
 * what was shared under those ids until it is told. Told here, after the
 * allocation table's lock is dropped, because telling it waits for an answer.
 * A device that is not there has nothing left to hold. */
void Adapter::release_shared()
{
    AV_SCOPE("Adapter::release_shared");
    constexpr uint32_t batch = 16u;
    uint64_t           ids[batch] = {};
    for (uint32_t taken = shared_.take_released(ids, batch); taken != 0u; taken = shared_.take_released(ids, batch)) {
        VirtioGpuDevice *const device = answering_device();
        for (uint32_t index = 0u; device != nullptr && index < taken; index++) {
            const Status released = av_contexts_.release_shared(ids[index], *device);
            if (released != Status::ok && budget_.allows())
                log_status("avgpu: a shared object's release", NtStatus(released).value());
        }
    }
}

int32_t Adapter::describe_allocation(DxgkargDescribeAllocationRecord *request)
{
    AV_SCOPE("Adapter::describe_allocation");
    platform_.require_irql_at_most(Irql::passive, "Adapter::describe_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t described = NtStatus(allocations_.describe(*request, sources_)).value();
    log_failure("avgpu: DescribeAllocation", described);
    return described;
}

int32_t Adapter::get_standard_allocation_driver_data(DxgkargGetStandardAllocationDriverDataRecord *request)
{
    AV_SCOPE("Adapter::get_standard_allocation_driver_data");
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_standard_allocation_driver_data");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t described = NtStatus(StandardAllocation(platform_, *request).describe()).value();
    log_status("avgpu: GetStandardAllocationDriverData", described);
    return described;
}

int32_t Adapter::open_allocation(void *device, DxgkargOpenAllocationRecord *request)
{
    AV_SCOPE("Adapter::open_allocation");
    platform_.require_irql_at_most(Irql::passive, "Adapter::open_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t opened = NtStatus(devices_.open(device, *request)).value();
    /* dxgkrnl runs an open inside every create, so a refusal here is one a
     * caller reads as a failed CreateAllocation. This was the only DDI in the
     * driver with no line, and it cost four guest boots (research.md §15). */
    log_status("avgpu: OpenAllocation", opened);
    return opened;
}

int32_t Adapter::close_allocation(void *device, const DxgkargCloseAllocationRecord *request)
{
    AV_SCOPE("Adapter::close_allocation");
    platform_.require_irql_at_most(Irql::passive, "Adapter::close_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(devices_.close(device, *request)).value();
}

int32_t Adapter::set_display_private_driver_format(const DxgkargSetDisplayPrivateDriverFormatRecord *request)
{
    AV_SCOPE("Adapter::set_display_private_driver_format");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_display_private_driver_format");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (request->PrimaryAllocation != nullptr && allocations_.find(request->PrimaryAllocation) == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    const int32_t set =
        NtStatus(sources_.set_private_format(request->VidPnSourceId, request->PrivateDriverFormatAttribute)).value();
    log_status("avgpu: SetDisplayPrivateDriverFormat", set);
    return set;
}

int32_t Adapter::create_context(void *device, DxgkargCreateContextRecord *request)
{
    AV_SCOPE("Adapter::create_context");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_context");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    /* Nothing new runs on a device this adapter did not start on. */
    if (identity_.changed()) return NtStatus(Status::not_supported).value();
    const Status created = contexts_.create(device, *request);
    Context *const context = created == Status::ok ? contexts_.find(request->hContext) : nullptr;
    if (context == nullptr || !context->is_av()) return NtStatus(created).value();
    const Status bound = bind_av(*context);
    if (bound == Status::ok) return NtStatus(Status::ok).value();
    static_cast<void>(contexts_.destroy(request->hContext));
    request->hContext = nullptr;
    /* dxgkrnl passes a CreateContext status it does not expect straight to the
     * process and logs it as invalid; the reason is in the driver's log. */
    return NtStatus(Status::invalid_parameter).value();
}

Status Adapter::bind_av(Context &context)
{
    AV_SCOPE("Adapter::bind_av");
    if (!doorbells_.constructed() || answering_device() == nullptr) return Status::not_supported;
    CapsetInfo     capset;
    CapsetExchange exchange(platform_, device_.object());
    const Status   read = exchange.read(capset);
    if (read != Status::ok) return read;
    return av_contexts_.bind(&context, context.av_record(), device_.object(), doorbells_.object(), capset);
}

VirtioGpuDevice *Adapter::answering_device()
{
    AV_SCOPE("Adapter::answering_device");
    /* A device is only ever constructed running: power_up destroys one whose start failed. */
    return device_.constructed() && present_ ? &device_.object() : nullptr;
}

int32_t Adapter::destroy_context(void *context)
{
    AV_SCOPE("Adapter::destroy_context");
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_context");
    /* DXGCONTEXT::DestroyContext destroys a context's queues first. */
    if (hw_queues_.serves(context)) return NtStatus(Status::invalid_parameter).value();
    const Context *const found = contexts_.find(context);
    /* Only one the table still holds. Freeing an allocation a context runs on
     * unbinds it there and then, and that is the order a process exit uses -
     * dxgkrnl frees the allocations before the context - so asking again here
     * would answer invalid_parameter and log a failure for the ordinary case. */
    if (found != nullptr && found->is_av() && av_contexts_.holds(context)) {
        log_status("avgpu: av context destroyed", NtStatus(av_contexts_.unbind(context, answering_device())).value());
    }
    return NtStatus(contexts_.destroy(context)).value();
}

int32_t Adapter::render(void *context, DxgkargRenderRecord *request)
{
    AV_SCOPE("Adapter::render");
    platform_.require_irql_at_most(Irql::passive, "Adapter::render");
    if (request == nullptr || contexts_.find(context) == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(RenderRequest(platform_, *request).render()).value();
}

int32_t Adapter::patch(const DxgkargPatchRecord *request)
{
    AV_SCOPE("Adapter::patch");
    platform_.require_irql_at_most(Irql::passive, "Adapter::patch");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(PatchRequest(platform_, *request).patch()).value();
}

int32_t Adapter::submit_command(const DxgkargSubmitCommandRecord *request)
{
    AV_SCOPE("Adapter::submit_command");
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::submit_command");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    const Status status = node_.submit(request->SubmissionFenceId);
    if (status == Status::ok) static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(status).value();
}

int32_t Adapter::preempt_command(const DxgkargPreemptCommandRecord *request)
{
    AV_SCOPE("Adapter::preempt_command");
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::preempt_command");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    const Status status = node_.preempt(request->PreemptionFenceId);
    static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(status).value();
}

int32_t Adapter::query_current_fence(DxgkargQueryCurrentFenceRecord *request)
{
    AV_SCOPE("Adapter::query_current_fence");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_current_fence");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    request->CurrentFence = node_.current_fence();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::build_paging_buffer(DxgkargBuildPagingBufferRecord *request)
{
    AV_SCOPE("Adapter::build_paging_buffer");
    platform_.require_irql_at_most(Irql::passive, "Adapter::build_paging_buffer");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    /* An unmap is VidMm evicting as often as a process exiting - a suspended
     * app's allocations go first - and ends no context: what a context runs on,
     * and every chunk it holds, is memory the driver owns, not an allocation
     * (docs/protocol.md, "Context memory"). */
    PagingRequest paging(platform_, allocations_, budget_, *request);
    return paging.build().value();
}

int32_t Adapter::reset_from_timeout()
{
    AV_SCOPE("Adapter::reset_from_timeout");
    platform_.require_irql_at_most(Irql::passive, "Adapter::reset_from_timeout");
    InvariantScope<Adapter> guard(platform_, *this, "reset_from_timeout");
    node_.reset();
    hw_submissions_.abandon();
    return NtStatus(power_down()).value();
}

int32_t Adapter::restart_from_timeout()
{
    AV_SCOPE("Adapter::restart_from_timeout");
    platform_.require_irql_at_most(Irql::passive, "Adapter::restart_from_timeout");
    InvariantScope<Adapter> guard(platform_, *this, "restart_from_timeout");
    const Status status = started_ ? power_up() : Status::ok;
    if (status != Status::ok) return NtStatus(status).value();
    node_.restart();
    return NtStatus(status).value();
}

int32_t Adapter::is_supported_vidpn(DxgkargIsSupportedVidPnRecord *request)
{
    AV_SCOPE("Adapter::is_supported_vidpn");
    platform_.require_irql_at_most(Irql::passive, "Adapter::is_supported_vidpn");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t decided = VidPnSupportCheck(platform_, dxgkrnl_, modes_, sources_, *request).decide().value();
    log_status("avgpu: IsSupportedVidPn", decided);
    return decided;
}

int32_t Adapter::recommend_functional_vidpn(const DxgkargRecommendFunctionalVidPnRecord *)
{
    AV_SCOPE("Adapter::recommend_functional_vidpn");
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_functional_vidpn");
    const int32_t declined = NtStatus(NtStatus::no_recommended_functional_vidpn).value();
    log_status("avgpu: RecommendFunctionalVidPn", declined);
    return declined;
}

/* dxgkrnl repeats the VidPn DDIs while it settles a topology, so these lines
 * come out of the budget rather than one per call forever. */
void Adapter::log_handle(const char *step, const void *handle, int32_t status)
{
    AV_SCOPE("Adapter::log_handle");
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
    AV_SCOPE("Adapter::follow_output_count");
    platform_.require_irql_at_most(Irql::passive, "Adapter::follow_output_count");
    const Status status = sources_.set_count(outputs);
    if (status == Status::ok || !budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: the device reports ");
    message.append_decimal(outputs);
    message.append(" displays, offering ");
    message.append_decimal(sources_.count());
    platform_.log(message.text());
}

Status Adapter::follow_display_info()
{
    AV_SCOPE("Adapter::follow_display_info");
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
    const Status followed = follow_display_modes();
    if (followed != Status::ok) return followed;
    edid_.follow(modes_.preferred());
    log_unlike_outputs();
    return Status::ok;
}

Status Adapter::follow_display_modes()
{
    AV_SCOPE("Adapter::follow_display_modes");
    platform_.require_irql_at_most(Irql::passive, "Adapter::follow_display_modes");
    uint8_t          request[AvRequestWriter::max_request_size] = {};
    AvRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.display_modes());
    uint8_t        answer[DisplayModeSet::max_encoded_size] = {};
    uint32_t       written = 0u;
    DisplayModeSet offered;
    if (device_.object().ask_control(request, writer.length(), answer, sizeof answer, written) == Status::ok &&
        offered.decode(answer, written) == Status::ok && modes_.adopt(offered) == Status::ok) {
        log_offered_modes(offered.count());
        return Status::ok;
    }
    return modes_.follow(display_info_, 0u);
}

/* What the host offered and what came of it: a mode past what the EDID can
 * describe is dropped, and the line says how many were. */
void Adapter::log_offered_modes(uint32_t offered)
{
    AV_SCOPE("Adapter::log_offered_modes");
    if (!budget_.allows()) return;
    char       bytes[112];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: the host offers ");
    message.append_decimal(offered);
    message.append(" modes, ");
    message.append_decimal(modes_.count());
    message.append(" taken, preferred ");
    message.append_decimal(modes_.preferred().width());
    message.append("x");
    message.append_decimal(modes_.preferred().height());
    platform_.log(message.text());
}

/* A display event the interrupt saw: the device's event is cleared, and each
 * monitor unplugged and plugged back, which has Windows read its EDID and modes
 * again - the passive DDIs that answer them follow the device's new modes. */
void Adapter::announce_display_change(bool running)
{
    AV_SCOPE("Adapter::announce_display_change");
    if (!running || platform_.load_atomic(display_announced_) == 0u) return;
    platform_.store_atomic(display_announced_, 0u);
    if (device_.object().acknowledge_display_event() != Status::ok && budget_.allows())
        platform_.log("avgpu: the display event could not be cleared");
    for (uint32_t child = 0u; child < sources_.count(); child++) dxgkrnl_.replug(child);
}

/* Every output is offered the list the first scanout's rectangle allows, which
 * is right while the device gives them all the same rectangle - QEMU does. A
 * host that does not is worth a line, because the modes the other outputs are
 * offered are then not theirs. */
void Adapter::log_unlike_outputs()
{
    AV_SCOPE("Adapter::log_unlike_outputs");
    for (uint32_t source = 1u; source < sources_.count(); source++) {
        if (display_info_.width(source) == display_info_.width(0u) &&
            display_info_.height(source) == display_info_.height(0u))
            continue;
        if (!budget_.allows()) return;
        char       bytes[96];
        TextBuffer message(bytes, sizeof bytes);
        message.append("avgpu: display ");
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
    AV_SCOPE("Adapter::follow_display_change");
    platform_.require_irql_at_most(Irql::passive, "Adapter::follow_display_change");
    if (platform_.load_atomic(display_changed_) == 0u) return;
    platform_.store_atomic(display_changed_, 0u);
    log_status("avgpu: display info", NtStatus(follow_display_info()).value());
}

/* The private-data length shares the line because the length is the question a
 * guest sweep asks: a length this driver answered leaves a line naming it, and
 * one dxgkrnl refused before the call leaves no line at all. Without the
 * number the two are told apart by counting lines, which is not evidence
 * (feature 053). */
void Adapter::log_allocation(const DxgkargCreateAllocationRecord &request, int32_t status)
{
    AV_SCOPE("Adapter::log_allocation");
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: CreateAllocation bytes ");
    message.append_decimal(request.pAllocationInfo == nullptr ? 0u
                                                              : request.pAllocationInfo[0].PrivateDriverDataSize);
    message.append(" status 0x");
    message.append_hex(static_cast<uint32_t>(status), 8u);
    platform_.log(message.text());
}

void Adapter::log_failure(const char *step, int32_t status)
{
    AV_SCOPE("Adapter::log_failure");
    if (status >= 0) return;
    log_status(step, status);
}

void Adapter::log_status(const char *step, int32_t status)
{
    AV_SCOPE("Adapter::log_status");
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
    AV_SCOPE("Adapter::enum_vidpn_cofunc_modality");
    platform_.require_irql_at_most(Irql::passive, "Adapter::enum_vidpn_cofunc_modality");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t enumerated = CofuncModality(platform_, dxgkrnl_, budget_, modes_, sources_, *request).enumerate().value();
    log_status("avgpu: EnumVidPnCofuncModality", enumerated);
    return enumerated;
}

int32_t Adapter::commit_vidpn(const DxgkargCommitVidPnRecord *request)
{
    AV_SCOPE("Adapter::commit_vidpn");
    platform_.require_irql_at_most(Irql::passive, "Adapter::commit_vidpn");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t applied = VidPnCommit(platform_, dxgkrnl_, modes_, sources_, *request).apply().value();
    if (applied >= 0) {
        const Status followed = follow_commit(request->AffectedVidPnSourceId);
        if (followed != Status::ok) log_status("avgpu: CommitVidPn scanout", NtStatus(followed).value());
    }
    log_status("avgpu: CommitVidPn", applied);
    return applied;
}

uint32_t Adapter::scanning_out() const
{
    AV_SCOPE("Adapter::scanning_out");
    uint32_t sources = 0u;
    for (uint32_t source = 0u; source < sources_.count(); source++)
        if (sources_.committed_mode(source) != nullptr) sources |= 1u << source;
    return sources;
}

void Adapter::restore_scanouts()
{
    AV_SCOPE("Adapter::restore_scanouts");
    for (uint32_t source = 0u; source < sources_.count(); source++) {
        if (!transition_.scanning_out(source)) continue;
        /* Said for every source power put back, because a resume that restored
         * one and not the other is the difference between a desktop and a
         * black screen. */
        log_status("avgpu: resume scanout", NtStatus(follow_commit(source)).value());
    }
}

Status Adapter::follow_commit(uint32_t source)
{
    AV_SCOPE("Adapter::follow_commit");
    const DisplayMode *mode = sources_.committed_mode(source);
    return mode != nullptr ? scanout_.configure(source, *mode) : scanout_.disable(source);
}

int32_t Adapter::update_active_vidpn_present_path(const D3dkmdtVidPnPresentPathRecord *path)
{
    AV_SCOPE("Adapter::update_active_vidpn_present_path");
    platform_.require_irql_at_most(Irql::passive, "Adapter::update_active_vidpn_present_path");
    if (path == nullptr || path->VidPnSourceId >= sources_.count() ||
        path->VidPnTargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    return PathTransformation(*path).accepted().value();
}

int32_t Adapter::recommend_monitor_modes(const DxgkargRecommendMonitorModesRecord *request)
{
    AV_SCOPE("Adapter::recommend_monitor_modes");
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_monitor_modes");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    follow_display_change();
    const int32_t recommended = MonitorModes(platform_, modes_, sources_, *request).recommend().value();
    log_status("avgpu: RecommendMonitorModes", recommended);
    return recommended;
}

int32_t Adapter::recommend_vidpn_topology(const DxgkargRecommendVidPnTopologyRecord *)
{
    AV_SCOPE("Adapter::recommend_vidpn_topology");
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_vidpn_topology");
    const int32_t declined = NtStatus(NtStatus::no_recommended_vidpn_topology).value();
    log_status("avgpu: RecommendVidPnTopology", declined);
    return declined;
}

/* Target 0 shows source 0. */
int32_t Adapter::get_scan_line(DxgkargGetScanLineRecord *request)
{
    AV_SCOPE("Adapter::get_scan_line");
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_scan_line");
    if (request == nullptr || request->VidPnTargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    const DisplayMode *mode = sources_.committed_mode(request->VidPnTargetId);
    if (mode == nullptr) return NtStatus(Status::invalid_state).value();
    const uint32_t line = vsync_.scan_line(*mode, platform_.interrupt_time());
    request->InVerticalBlank = line >= mode->height() ? 1u : 0u;
    request->ScanLine = line;
    return NtStatus(Status::ok).value();
}

/* Before StartDevice there is no timer to start. */
int32_t Adapter::control_interrupt(int32_t type, uint8_t enable)
{
    AV_SCOPE("Adapter::control_interrupt");
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
    AV_SCOPE("Adapter::set_vidpn_source_address");
    platform_.require_irql_at_most(Irql::isr, "Adapter::set_vidpn_source_address");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (request->hAllocation == nullptr) return flipped(sources_.set_scanout(request->VidPnSourceId, 0, nullptr));
    return show_primary(request->VidPnSourceId, request->PrimarySegment, request->PrimaryAddress, request->hAllocation);
}

int32_t Adapter::show_primary(uint32_t source, uint32_t segment, int64_t address, void *allocation)
{
    AV_SCOPE("Adapter::show_primary");
    if (segment != ApertureSegment::segment_id && segment != system_memory_driver_segment)
        return NtStatus(Status::invalid_parameter).value();
    log_scanout(source, segment, address);
    return flipped(sources_.set_scanout(source, address, allocation));
}

/* A flip reaches the host from the DPC, and the device's clock shows only
 * what it has before its blank: the DPC is asked for now rather than left to
 * whatever interrupt comes next. */
int32_t Adapter::flipped(Status set)
{
    AV_SCOPE("Adapter::flipped");
    if (set == Status::ok) static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(set).value();
}

/* Which source dxgkrnl showed or hid, and not only that it asked: a display the
 * power policy turned off comes back through this, and the log is the only way
 * to see whether it did. */
void Adapter::log_visibility(const DxgkargSetVidPnSourceVisibilityRecord &request, int32_t status)
{
    AV_SCOPE("Adapter::log_visibility");
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: SetVidPnSourceVisibility source ");
    message.append_decimal(request.VidPnSourceId);
    message.append(request.Visible != 0u ? " visible" : " hidden");
    message.append(" status 0x");
    message.append_hex(static_cast<uint32_t>(status), 8u);
    platform_.log(message.text());
}

void Adapter::log_scanout(uint32_t source, uint32_t segment, int64_t address)
{
    AV_SCOPE("Adapter::log_scanout");
    if (platform_.current_irql() >= Irql::dispatch || !budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: SetVidPnSourceAddress source ");
    message.append_decimal(source);
    message.append(" segment ");
    message.append_decimal(segment);
    message.append(" address 0x");
    message.append_hex(static_cast<uint64_t>(address), 16u);
    platform_.log(message.text());
}

int32_t Adapter::set_vidpn_source_visibility(const DxgkargSetVidPnSourceVisibilityRecord *request)
{
    AV_SCOPE("Adapter::set_vidpn_source_visibility");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_vidpn_source_visibility");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const int32_t set = NtStatus(sources_.set_visible(request->VidPnSourceId, request->Visible != 0u)).value();
    log_visibility(*request, set);
    return set;
}

int32_t Adapter::present(void *context, DxgkargPresentRecord *request)
{
    AV_SCOPE("Adapter::present");
    platform_.require_irql_at_most(Irql::passive, "Adapter::present");
    if (request == nullptr || contexts_.find(context) == nullptr) return NtStatus(Status::invalid_parameter).value();
    PresentRequest writing(platform_, *request);
    platform_.store_atomic(presents_made_, platform_.load_atomic(presents_made_) + 1u);   /* a lost race still moves it */
    const uint32_t first = request->MultipassOffset;
    const int32_t  presented = writing.present().value();
    /* The DMA buffer this wrote is executed on the host only for a context of
     * the av protocol; dxgkrnl's own presents are loopback-completed, so the
     * pixels the desktop presents move here (feature 046, FR-004). */
    /* A refusal is what dxgkrnl answers by hiding the source: the only trace
     * of why the desktop went dark is this line. More DMA buffer is not one. */
    PresentOperation named = PresentOperation::blt;
    if (presented < 0 && presented != more_dma_buffer)
        log_refused_present(*request, writing.operation(named) == Status::ok);
    /* Each pass moves the sub-rectangles its DMA buffer carries, and its
     * packet waits for them; a pass that found no room wrote none. */
    const uint32_t end = presented == 0 ? request->SubRectCnt : request->MultipassOffset;
    if (presented == 0 || (presented == more_dma_buffer && end != first)) {
        PresentOperation kind = PresentOperation::blt;
        /* A present that was written named an operation this driver presents:
         * `present` refuses every other one before it writes anything. */
        static_cast<void>(writing.operation(kind));
        mark_present(*request, move_presented_pixels(*request, kind, first, end));
    }
    log_failure("avgpu: Present", presented);
    return presented;
}

/* The pixels a blt or a fill names move here, whichever form names them
 * (PresentedAllocations): dxgkrnl's own presents, and an application's
 * blt-model present into its window's redirection surface. */
uint64_t Adapter::move_presented_pixels(const DxgkargPresentRecord &request, PresentOperation kind, uint32_t first,
                                       uint32_t end)
{
    AV_SCOPE("Adapter::move_presented_pixels");
    const bool filling = kind == PresentOperation::color_fill;
    if (kind == PresentOperation::flip) return 0u;
    const PresentedAllocations named(request);
    void *const                source_handle = filling ? nullptr : named.source();
    void *const                destination_handle = named.destination();
    if (destination_handle == nullptr || (!filling && source_handle == nullptr)) {
        log_unmoved_present("list", request, request.NumSrcAllocations, request.NumDstAllocations);
        return 0u;
    }
    DeviceAllocation *const source = filling ? nullptr : devices_.opened(source_handle);
    DeviceAllocation *const destination = devices_.opened(destination_handle);
    if (destination == nullptr || (!filling && source == nullptr)) {
        log_unmoved_present("handles", request, request.NumSrcAllocations, request.NumDstAllocations);
        return 0u;
    }
    const PresentSurface written(destination->allocation().descriptor().layout(), destination->allocation().pages());
    const PresentSurface read(source != nullptr ? source->allocation().descriptor().layout() : SurfaceLayout(),
                              source != nullptr ? source->allocation().pages() : GuestPageList());
    PresentTicket ticket(platform_, budget_, presents_.constructed() && present_ ? &presents_.object() : nullptr,
                         immediate_mover(), image_of(request, source));
    PresentBlit   blit(platform_, ticket, source != nullptr ? &read : nullptr, written);
    const Rectangle into(request.DstRect.left, request.DstRect.top, request.DstRect.right, request.DstRect.bottom);
    const uint32_t  count = end - first;
    const RectRecord *const rectangles = count != 0u ? request.pDstSubRects + first : nullptr;
    const Rectangle   from(request.SrcRect.left, request.SrcRect.top, request.SrcRect.right, request.SrcRect.bottom);
    const Status      moved = filling ? blit.fill(request.Color, into, rectangles, count)
                                      : blit.blit(from, into, rectangles, count);
    if (moved != Status::ok) {
        log_unmoved_present("surfaces", request, request.NumSrcAllocations, request.NumDstAllocations);
        log_unreadable_surfaces(read, written, source != nullptr ? source->allocation().descriptor().kind()
                                                                 : AllocationKind{},
                                destination->allocation().descriptor().kind(), from, into);
    }
    present_rows_ += blit.rows();
    return ticket.ticket();
}

/* The user-mode driver names a point only where the answer to its
 * UMDRIVERPRIVATE said it may; one named anyway on a device that reads none is
 * a present from its pages, which is all such a device has. The surface is
 * the one the source allocation was minted as - the driver's private data
 * names only when, never what. */
SurfacePoint Adapter::image_of(const DxgkargPresentRecord &request, const DeviceAllocation *source)
{
    AV_SCOPE("Adapter::image_of");
    PresentPointRecord record;
    if (source == nullptr || !identity_.offers(CapsetInfo::feature_present_from_image) ||
        record.decode(static_cast<const uint8_t *>(request.pPrivateDriverData), request.PrivateDriverDataSize) !=
            Status::ok)
        return SurfacePoint();
    return SurfacePoint(source->allocation().shared_id(), record.point());
}

PixelMover &Adapter::immediate_mover()
{
    AV_SCOPE("Adapter::immediate_mover");
    if (copier_.constructed() && present_) return copier_.object();
    return mapped_pixels_;
}

/* dxgkrnl hands the private data on to SubmitCommandVirtual with the packet
 * this present's DMA buffer becomes, from where the pointer started to where
 * it was left, as it does the DMA buffer's bytes. */
void Adapter::mark_present(DxgkargPresentRecord &request, uint64_t ticket)
{
    AV_SCOPE("Adapter::mark_present");
    if (ticket == 0u) return;
    auto *const private_data = static_cast<uint8_t *>(request.pDmaBufferPrivateData);
    if (private_data == nullptr || PresentTicketRecord(presents_.object().epoch(), ticket)
                                           .encode(private_data, request.DmaBufferPrivateDataSize) != Status::ok) {
        log_unticketed_present(request);
        return;
    }
    request.pDmaBufferPrivateData = private_data + PresentTicketRecord::encoded_size;
    request.DmaBufferPrivateDataSize -= static_cast<uint32_t>(PresentTicketRecord::encoded_size);
}

void Adapter::log_unticketed_present(const DxgkargPresentRecord &request)
{
    AV_SCOPE("Adapter::log_unticketed_present");
    presents_unticketed_++;
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: present unticketed private data ");
    message.append(request.pDmaBufferPrivateData != nullptr ? "given" : "absent");
    message.append(" bytes ");
    message.append_decimal(request.DmaBufferPrivateDataSize);
    platform_.log(message.text());
}

void Adapter::log_refused_present(const DxgkargPresentRecord &request, bool operation_named)
{
    AV_SCOPE("Adapter::log_refused_present");
    if (!budget_.allows()) return;
    char       bytes[224];
    TextBuffer message(bytes, sizeof bytes);
    message.append(operation_named ? "avgpu: present refused flags 0x" : "avgpu: present names no operation flags 0x");
    message.append_hex(request.Flags, 8u);
    message.append(" list ");
    message.append(request.pAllocationList != nullptr ? "given" : "absent");
    message.append(request.pDmaBuffer != nullptr ? " dma" : " no-dma");
    message.append(" rects ");
    message.append_decimal(request.MultipassOffset);
    message.append("/");
    message.append_decimal(request.SubRectCnt);
    const RectRecord *const rects[2] = {&request.SrcRect, &request.DstRect};
    for (const RectRecord *rect : rects) {
        message.append(" ");
        const int32_t sides[4] = {rect->left, rect->top, rect->right, rect->bottom};
        for (int32_t side : sides) {
            message.append(side < 0 ? "-" : "");
            message.append_decimal(static_cast<uint32_t>(side < 0 ? -side : side));
            message.append(",");
        }
    }
    platform_.log(message.text());
}

/* Which of a blt's surfaces the miniport could not read or write - a surface
 * whose pages VidMm never mapped into the aperture - or the two rectangles it
 * would have had to stretch between. */
void Adapter::log_unreadable_surfaces(const PresentSurface &read, const PresentSurface &written,
                                      AllocationKind source_kind, AllocationKind destination_kind,
                                      const Rectangle &from, const Rectangle &into)
{
    AV_SCOPE("Adapter::log_unreadable_surfaces");
    if (!budget_.allows()) return;
    char       bytes[160];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: present surfaces reachable source ");
    message.append_decimal(static_cast<uint32_t>(read.reachable()));
    message.append(" kind ");
    message.append_decimal(static_cast<uint32_t>(source_kind));
    message.append(" destination ");
    message.append_decimal(static_cast<uint32_t>(written.reachable()));
    message.append(" kind ");
    message.append_decimal(static_cast<uint32_t>(destination_kind));
    const Rectangle *const sides[2] = {&from, &into};
    for (const Rectangle *side : sides) {
        message.append(side == &from ? " from " : " into ");
        message.append_decimal(static_cast<uint32_t>(side->right() - side->left()));
        message.append("x");
        message.append_decimal(static_cast<uint32_t>(side->bottom() - side->top()));
    }
    platform_.log(message.text());
}

/* A present whose pixels stayed where they were, with what it was: the desktop
 * not reaching the screen is invisible from the guest, so the driver says so
 * once per present it could not carry. */
void Adapter::log_unmoved_present(const char *why, const DxgkargPresentRecord &request, uint32_t sources,
                                  uint32_t destinations)
{
    AV_SCOPE("Adapter::log_unmoved_present");
    presents_unmoved_++;
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: present unmoved ");
    message.append(why);
    message.append(" flags 0x");
    message.append_hex(request.Flags, 8u);
    message.append(" sources ");
    message.append_decimal(sources);
    message.append(" destinations ");
    message.append_decimal(destinations);
    platform_.log(message.text());
}

int32_t Adapter::set_pointer_shape(const DxgkargSetPointerShapeRecord *request)
{
    AV_SCOPE("Adapter::set_pointer_shape");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_pointer_shape");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(cursor_.set_shape(*request)).value();
}

int32_t Adapter::set_pointer_position(const DxgkargSetPointerPositionRecord *request)
{
    AV_SCOPE("Adapter::set_pointer_position");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_pointer_position");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(cursor_.set_position(*request)).value();
}

int32_t Adapter::acquire_swizzling_range(void *)
{
    AV_SCOPE("Adapter::acquire_swizzling_range");
    platform_.require_irql_at_most(Irql::passive, "Adapter::acquire_swizzling_range");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::release_swizzling_range(const void *)
{
    AV_SCOPE("Adapter::release_swizzling_range");
    platform_.require_irql_at_most(Irql::passive, "Adapter::release_swizzling_range");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::set_palette(const void *)
{
    AV_SCOPE("Adapter::set_palette");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_palette");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::stop_capture(const void *)
{
    AV_SCOPE("Adapter::stop_capture");
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_capture");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::create_overlay(void *)
{
    AV_SCOPE("Adapter::create_overlay");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_overlay");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::query_vidpn_hw_capability(DxgkargQueryVidPnHwCapabilityRecord *request)
{
    AV_SCOPE("Adapter::query_vidpn_hw_capability");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_vidpn_hw_capability");
    if (request == nullptr || request->hFunctionalVidPn == nullptr ||
        request->SourceId >= sources_.count() || request->TargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    request->VidPnHWCaps = 0u;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_dependent_engine_group(DxgkargQueryDependentEngineGroupRecord *request)
{
    AV_SCOPE("Adapter::query_dependent_engine_group");
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_dependent_engine_group");
    if (request == nullptr || request->NodeOrdinal >= DriverCapabilities::node_count ||
        request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    /* Each node depends on itself alone: what runs on one does not hold the
     * other up. */
    request->DependentNodeOrdinalMask = uint64_t{1} << request->NodeOrdinal;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_engine_status(DxgkargQueryEngineStatusRecord *request)
{
    AV_SCOPE("Adapter::query_engine_status");
    /* VidSch asks from its timeout DPC: DdiQueryEngineStatus (dxgkrnl.sys
     * @140027dc0) only reaches for the current thread when IRQL is below 2, so
     * dispatch is a level it expects to be called at. */
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::query_engine_status");
    if (request == nullptr || request->NodeOrdinal >= DriverCapabilities::node_count ||
        request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    /* VidSchiCheckNodeTimeout takes responsive as "no timeout this round". The
     * hardware-scheduled node runs no packets, so nothing there can be stuck. */
    if (request->NodeOrdinal != DriverCapabilities::packet_node || progress_.responsive())
        request->EngineStatus |= engine_responsive;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::reset_engine(DxgkargResetEngineRecord *request)
{
    AV_SCOPE("Adapter::reset_engine");
    /* The same DPC resets what it found stuck; DdiResetEngine (dxgkrnl.sys
     * @14006e0a4) checks only that the driver returns at the IRQL it entered. */
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::reset_engine");
    if (request == nullptr || request->NodeOrdinal >= DriverCapabilities::node_count ||
        request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    /* VidSch already knows what it reset, so nothing more is reported; every
     * other context keeps running - a reset of one engine is not an outage
     * (FR-002, FR-003). Resetting the hardware-scheduled node drops what its
     * queues had in flight, which is what ResetHwEngine does for it. */
    if (request->NodeOrdinal != DriverCapabilities::packet_node) {
        hw_submissions_.abandon();
        request->LastAbortedFenceId = 0u;
        return NtStatus(Status::ok).value();
    }
    request->LastAbortedFenceId = node_.abort_pending();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::cancel_command(const DxgkargCancelCommandRecord *request)
{
    AV_SCOPE("Adapter::cancel_command");
    platform_.require_irql_at_most(Irql::passive, "Adapter::cancel_command");
    if (request == nullptr || contexts_.find(request->hContext) == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::notify_surprise_removal(int32_t removal_type)
{
    AV_SCOPE("Adapter::notify_surprise_removal");
    platform_.require_irql_at_most(Irql::passive, "Adapter::notify_surprise_removal");
    if (removal_type != removal_hibernation && removal_type != removal_pnp_notify)
        return NtStatus(Status::invalid_parameter).value();
    present_ = false;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::stop_device_and_release_post_display_ownership(uint32_t target, DxgkDisplayInformationRecord *display)
{
    AV_SCOPE("Adapter::stop_device_and_release_post_display_ownership");
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_device_and_release_post_display_ownership");
    if (display == nullptr || target >= sources_.count()) return NtStatus(Status::invalid_parameter).value();
    *display = DxgkDisplayInformationRecord{};
    return stop_device();
}

int32_t Adapter::get_child_container_id(uint32_t child, DxgkChildContainerIdRecord *container)
{
    AV_SCOPE("Adapter::get_child_container_id");
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_child_container_id");
    if (container == nullptr || child >= sources_.count()) return NtStatus(Status::invalid_parameter).value();
    edid_.describe(*container);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::system_display_enable(uint32_t, void *, uint32_t *, uint32_t *, int32_t *)
{
    AV_SCOPE("Adapter::system_display_enable");
    platform_.require_irql_at_most(Irql::isr, "Adapter::system_display_enable");
    return NtStatus(Status::not_supported).value();
}

void Adapter::system_display_write(const void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
{
    AV_SCOPE("Adapter::system_display_write");
    platform_.require_irql_at_most(Irql::isr, "Adapter::system_display_write");
}

int32_t Adapter::set_power_component_fstate(uint32_t, uint32_t)
{
    AV_SCOPE("Adapter::set_power_component_fstate");
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::set_power_component_fstate");
    return NtStatus(Status::invalid_parameter).value();
}

int32_t Adapter::power_runtime_control_request(const void *, const void *, size_t, void *, size_t, size_t *)
{
    AV_SCOPE("Adapter::power_runtime_control_request");
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::power_runtime_control_request");
    return NtStatus(Status::not_supported).value();
}

/* dxgkrnl flips through this instead of SetVidPnSourceAddress once the
 * compositor has flipped a multiplane present, and a failure here is a 0x119
 * bugcheck (FAILED_SETVIDPNSOURCEMPO_COMMAND), not a refused flip. This
 * adapter scans out one plane, the base one: it is shown as SetVidPnSourceAddress
 * shows a primary, and a flip with no enabled base plane - dxgmms2's
 * disable-planes flip when the primary's device goes away
 * (VidSchiSubmitDisablePlanesFlip) - shows nothing. An overlay plane above
 * the base is not composed. The vsync that follows reports the flip as
 * multiplane, the only report that retires it. */
int32_t Adapter::set_vidpn_source_address_with_multi_plane_overlay(const DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord *request)
{
    AV_SCOPE("Adapter::set_vidpn_source_address_with_multi_plane_overlay");
    platform_.require_irql_at_most(Irql::isr, "Adapter::set_vidpn_source_address_with_multi_plane_overlay");
    if (request == nullptr || (request->PlaneCount != 0u && request->pPlanes == nullptr))
        return NtStatus(Status::invalid_parameter).value();
    const DxgkMultiplaneOverlayPlaneRecord *base = nullptr;
    for (uint32_t index = 0u; index < request->PlaneCount; index++) {
        const DxgkMultiplaneOverlayPlaneRecord &plane = request->pPlanes[index];
        if (plane.LayerIndex == 0u && plane.Enabled != 0u && plane.hAllocation != nullptr) base = &plane;
    }
    if (base != nullptr && base->AllocationSegment != ApertureSegment::segment_id &&
        base->AllocationSegment != system_memory_driver_segment)
        return NtStatus(Status::invalid_parameter).value();
    if (base != nullptr) log_scanout(request->VidPnSourceId, base->AllocationSegment, base->AllocationAddress);
    return flipped(sources_.set_multiplane_scanout(request->VidPnSourceId, base));
}

/* Registering this DDI is what makes dxgkrnl answer KMTQAITYPE_MPO3DDI_SUPPORT
 * (adapter[0xc01], DxgkQueryAdapterInfoImpl), and that answer is what keeps
 * DWM's DisplayCore path off its legacy present, which is E_NOTIMPL
 * (COverlayContext::LegacyPresentRequired, dwmcore.dll); dxgmms2 then flips
 * every primary and overlay plane through it. Each plane named replaces the
 * one at its layer: dxgmms2 sends a layer only when it changes
 * (VidSchiSetupMmIoFlipMultiPlaneOverlay3), a turned-off one included.
 * dxgkrnl takes only success, busy or retry from it, so a plane this adapter
 * cannot show - CheckMultiPlaneOverlaySupport3 refused it, so dxgkrnl should
 * never send one - is shown as nothing rather than refused. The vsync that
 * follows names every layer's PresentId, the only report that retires an MPO3
 * flip. An immediate flip completes when this returns, which it does. */
int32_t Adapter::set_vidpn_source_address_with_multi_plane_overlay3(
    DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record *request)
{
    AV_SCOPE("Adapter::set_vidpn_source_address_with_multi_plane_overlay3");
    platform_.require_irql_at_most(Irql::isr, "Adapter::set_vidpn_source_address_with_multi_plane_overlay3");
    if (request == nullptr || (request->PlaneCount != 0u && request->ppPlanes == nullptr))
        return NtStatus(Status::invalid_parameter).value();
    OverlayPlane planes[OverlayPlane::max_planes];
    uint32_t     count = 0u;
    for (uint32_t index = 0u; index < request->PlaneCount && count < OverlayPlane::max_planes; index++) {
        const DxgkMultiplaneOverlayPlane3Record *const plane = request->ppPlanes[index];
        if (plane == nullptr || plane->LayerIndex >= OverlayPlane::max_planes) continue;
        const DxgkPrimaryContextDataRecord *shown = nullptr;
        if ((plane->InputFlags & plane_input_enabled) != 0u && plane->ContextCount != 0u &&
            plane->ppContextData != nullptr)
            shown = plane->ppContextData[0];
        if (shown != nullptr && (shown->hAllocation == nullptr || (shown->SegmentId != ApertureSegment::segment_id &&
                                                                   shown->SegmentId != system_memory_driver_segment)))
            shown = nullptr;
        if (shown != nullptr && plane->LayerIndex == 0u)
            log_scanout(request->VidPnSourceId, shown->SegmentId, shown->SegmentAddress);
        planes[count++] = shown != nullptr ? OverlayPlane(plane->LayerIndex, plane->PresentId, shown->hAllocation,
                                                          shown->SegmentAddress, plane->PlaneAttributes)
                                           : OverlayPlane(plane->LayerIndex, plane->PresentId);
    }
    return flipped(sources_.present_planes(request->VidPnSourceId, planes, count));
}

int32_t Adapter::check_multi_plane_overlay_support3(DxgkargCheckMultiPlaneOverlaySupport3Record *request)
{
    AV_SCOPE("Adapter::check_multi_plane_overlay_support3");
    platform_.require_irql_at_most(Irql::passive, "Adapter::check_multi_plane_overlay_support3");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const Status checked = overlays_.check(*request);
    if (checked == Status::ok && request->Supported == 0) log_refused_overlay(*request);
    return NtStatus(checked).value();
}

/* A configuration refused is a window DWM composes instead of promoting, and
 * nothing else says why: the plane that failed, what it is and where. */
void Adapter::log_refused_overlay(const DxgkargCheckMultiPlaneOverlaySupport3Record &request)
{
    AV_SCOPE("Adapter::log_refused_overlay");
    if (!budget_.allows()) return;
    const uint32_t failing = request.ReturnInfo & check_failing_plane_mask;
    char           bytes[224];
    TextBuffer     message(bytes, sizeof bytes);
    message.append("avgpu: overlay refused, plane ");
    message.append_decimal(failing);
    message.append(" of ");
    message.append_decimal(request.PlaneCount);
    const DxgkMultiplaneOverlayPlaneWithSource2Record *const plane =
        failing < request.PlaneCount ? request.ppPlanes[failing] : nullptr;
    if (plane != nullptr) {
        const auto *const allocation = static_cast<const Allocation *>(plane->hAllocation);
        const DxgkMultiplaneOverlayAttributes3Record &shown = plane->PlaneAttributes;
        message.append(" layer ");
        message.append_decimal(plane->LayerIndex);
        message.append(" format 0x");
        message.append_hex(allocation != nullptr ? allocation->descriptor().layout().format() : 0u, 8u);
        message.append(allocation != nullptr && allocation->host_key() != 0u ? " keyed" : " unkeyed");
        message.append(" space ");
        message.append_decimal(shown.ColorSpaceType);
        message.append(" rotation ");
        message.append_decimal(shown.Rotation);
        message.append(" flags 0x");
        message.append_hex(shown.Flags, 2u);
        message.append(" src ");
        message.append_decimal(static_cast<uint32_t>(shown.SrcRect.right - shown.SrcRect.left));
        message.append("x");
        message.append_decimal(static_cast<uint32_t>(shown.SrcRect.bottom - shown.SrcRect.top));
        message.append(" dst ");
        message.append_decimal(static_cast<uint32_t>(shown.DstRect.right - shown.DstRect.left));
        message.append("x");
        message.append_decimal(static_cast<uint32_t>(shown.DstRect.bottom - shown.DstRect.top));
    } else if (request.PostCompositionCount != 0u) {
        message.append(" (the post-composition)");
    }
    platform_.log(message.text());
}

int32_t Adapter::get_multi_plane_overlay_caps(DxgkargGetMultiPlaneOverlayCapsRecord *request)
{
    AV_SCOPE("Adapter::get_multi_plane_overlay_caps");
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_multi_plane_overlay_caps");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(overlays_.caps(*request)).value();
}

int32_t Adapter::get_post_composition_caps(DxgkargGetPostCompositionCapsRecord *request)
{
    AV_SCOPE("Adapter::get_post_composition_caps");
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_post_composition_caps");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(overlays_.post_composition_caps(*request)).value();
}

int32_t Adapter::get_node_metadata(uint32_t node_and_adapter, DxgkNodeMetadataRecord *metadata)
{
    AV_SCOPE("Adapter::get_node_metadata");
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_node_metadata");
    /* The low half names the node, the high half the physical adapter, of
     * which this driver has one. */
    const uint32_t node = node_and_adapter & 0xFFFFu;
    if (metadata == nullptr || (node_and_adapter >> 16u) != 0u || node >= DriverCapabilities::node_count)
        return NtStatus(Status::invalid_parameter).value();
    static constexpr char names[DriverCapabilities::node_count][9] = {"3DNode00", "3DNode01"};
    const char *const     name = names[node];
    *metadata = DxgkNodeMetadataRecord{};
    metadata->EngineType = engine_type_3d;
    metadata->GpuMmuSupported = 1u;
    /* What this node is scheduled by, where the OS allows it: the queue DDIs
     * are implemented and tested (feature 019), and the depth is what this
     * driver can hold in flight. A node that reports context scheduling the OS
     * has not enabled fails the adapter start outright, so what it reports is
     * what the OS answered at StartDevice (research.md §15).
     *
     * The hardware-scheduled node is the second one, because dxgkrnl checks a
     * node's context scheduling against the node before it: it is satisfied by
     * an earlier node's GpuMmu support, and node 0 supplies that (§16). What
     * the second node reports is what the OS allowed and this driver
     * implements - the queue DDIs, the doorbell ones, and the native fences
     * the OS demands beside them (§14). */
    metadata->Flags = node == DriverCapabilities::hardware_queue_node && hardware_scheduling_
                          ? node_context_scheduling_supported |
                                (user_mode_submission_ ? node_user_mode_submission : 0u) |
                                (HwQueueSubmissions::capacity << node_in_flight_hw_queue_buffers_shift)
                          : 0u;
    for (uint32_t index = 0u; index + 1u < sizeof names[0]; index++)
        metadata->FriendlyName[index] = static_cast<uint16_t>(name[index]);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::set_power_pstate(uint32_t, uint32_t)
{
    AV_SCOPE("Adapter::set_power_pstate");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_power_pstate");
    return NtStatus(Status::invalid_parameter).value();
}

int32_t Adapter::control_interrupt2(DxgkargControlInterrupt2Record control)
{
    AV_SCOPE("Adapter::control_interrupt2");
    platform_.require_irql_at_most(Irql::passive, "Adapter::control_interrupt2");
    if (control.InterruptType != interrupt_crtc_vsync) return NtStatus(Status::not_supported).value();
    if (control.CrtcVsyncState < vsync_enable || control.CrtcVsyncState > vsync_disable_no_phase)
        return NtStatus(Status::invalid_parameter).value();
    return control_interrupt(control.InterruptType, control.CrtcVsyncState == vsync_enable ? 1u : 0u);
}

int32_t Adapter::calibrate_gpu_clock(uint32_t node, uint32_t engine, DxgkGpuClockDataRecord *clock)
{
    AV_SCOPE("Adapter::calibrate_gpu_clock");
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
    AV_SCOPE("Adapter::submit_command_virtual");
    platform_.require_irql_at_most(Irql::passive, "Adapter::submit_command_virtual");
    if (request == nullptr || request->NodeOrdinal != 0u) return NtStatus(Status::invalid_parameter).value();
    const Context *context = contexts_.find(request->hContext);
    if (context == nullptr || !context->admits(request->DmaBufferVirtualAddress))
        return NtStatus(Status::invalid_parameter).value();
    /* The device that would have run this is not the one this adapter started
     * on, so no completion would ever come; SubmitCommandVirtual may refuse,
     * and D3DKMTSubmitCommand fails for the process instead (FR-005). */
    if (identity_.changed()) return NtStatus(Status::invalid_parameter).value();
    uint64_t     awaited = 0u;
    void        *owner = request->hContext;
    /* Every ordinary context is given room for a FollowRecord, so dxgkrnl
     * hands its private data to every packet: only one that decodes as a
     * record follows anything. */
    FollowRecord follow;
    const bool   follows =
        !context->is_av() && follow.decode(static_cast<const uint8_t *>(request->pDmaBufferPrivateData),
                                            request->DmaBufferPrivateDataSize) == Status::ok;
    Status       status = Status::ok;
    const bool   presented = submit_present(*request, status, owner, awaited);
    if (!presented)
        status = context->is_av() ? submit_av(*request, awaited)
                 : follows         ? submit_follow(*context, follow, request->SubmissionFenceId, owner, awaited)
                                   : node_.submit(request->SubmissionFenceId);
    if (status == Status::ok) {
        /* Only a batch the device already finished needs this DPC: its
         * interrupt may have come before the packet was queued. One still
         * running is reported by the interrupt it raises when it finishes,
         * and a DPC per submission doubled the work dxgkrnl's DPC lock
         * serializes. */
        const bool done = presented ? presents_.object().completed() >= awaited
                                    : av_contexts_.completed_already(owner, awaited);
        if (awaited == 0u || done) static_cast<void>(dxgkrnl_.queue_dpc());
        return NtStatus(Status::ok).value();
    }
    /* A refusal is what dxgkrnl answers the process with, and may be what puts
     * its device in error: the log is the only place the reason survives. */
    log_status("avgpu: SubmitCommandVirtual refused", NtStatus(status).value());
    return NtStatus(Status::invalid_parameter).value();
}

/* The host may finish the batch before its packet is queued; the DPC queued
 * after both serves what the fence page already says, so nothing waits for an
 * interrupt that came first. */
Status Adapter::submit_av(const DxgkargSubmitCommandVirtualRecord &request, uint64_t &awaited)
{
    AV_SCOPE("Adapter::submit_av");
    const Status published = av_contexts_.submit(request.hContext,
                                                  static_cast<const uint8_t *>(request.pDmaBufferPrivateData),
                                                  request.DmaBufferPrivateDataSize, awaited);
    if (published != Status::ok) return published;
    return node_.submit(request.SubmissionFenceId, request.hContext, awaited);
}

/* A packet on an ordinary context that follows a batch on one of its
 * process's av contexts: held on the node until that context's host has
 * completed the batch, like the av context's own packet would be. */
Status Adapter::submit_follow(const Context &context, const FollowRecord &follow, uint32_t fence, void *&owner,
                              uint64_t &awaited)
{
    AV_SCOPE("Adapter::submit_follow");
    Context *const followed = contexts_.followed(context, follow.ring());
    if (followed == nullptr) return Status::invalid_parameter;
    owner = followed;
    awaited = follow.fence();
    const Status queued = node_.submit(fence, followed, awaited);
    if (queued == Status::ok) av_contexts_.kick(followed);
    return queued;
}

/* Checked before the context's own kind: a present's DMA buffer comes to any
 * context, and its private data is the miniport's, written by DdiPresent. A
 * record that names no present of the queue's - another epoch, a ticket never
 * queued - holds nothing, so a process cannot park the node on one.
 * dxgkrnl counts the bytes DdiPresent advanced past as the UMD's private data
 * and passes only what was left after them as the private data's size: on a
 * context with room for the record alone that size is 0, and the packet ran
 * ahead of its pixels. */
bool Adapter::submit_present(const DxgkargSubmitCommandVirtualRecord &request, Status &status, void *&owner,
                             uint64_t &awaited)
{
    AV_SCOPE("Adapter::submit_present");
    PresentTicketRecord record;
    if (!presents_.constructed() ||
        record.decode(static_cast<const uint8_t *>(request.pDmaBufferPrivateData),
                      request.DmaBufferUmdPrivateDataSize) !=
            Status::ok ||
        !presents_.object().holds(record))
        return false;
    owner = &presents_.object();
    awaited = record.ticket();
    status = node_.submit(request.SubmissionFenceId, owner, awaited);
    return true;
}

void Adapter::set_root_page_table(const DxgkargSetRootPageTableRecord *request)
{
    AV_SCOPE("Adapter::set_root_page_table");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_root_page_table");
    Context *context = request != nullptr ? contexts_.find(request->hContext) : nullptr;
    if (context == nullptr || context->assign_root(*request) != Status::ok)
        platform_.log("avgpu: SetRootPageTable named no context or no usable root");
}

size_t Adapter::get_root_page_table_size(DxgkargGetRootPageTableSizeRecord *request)
{
    AV_SCOPE("Adapter::get_root_page_table_size");
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_root_page_table_size");
    return request != nullptr ? static_cast<size_t>(geometry_.root_table_bytes(request->NumberOfPte)) : 0u;
}

int32_t Adapter::create_process(DxgkargCreateProcessRecord *request)
{
    AV_SCOPE("Adapter::create_process");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_process");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    /* dxgkrnl's own handle, unique per process: what tells a present which
     * process's command streams it follows (Adapter::submit_command_virtual). */
    request->hKmdProcess = request->hDxgkProcess;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::destroy_process(void *)
{
    AV_SCOPE("Adapter::destroy_process");
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_process");
    return NtStatus(Status::ok).value();
}

void Adapter::set_stable_power_state(const DxgkargSetStablePowerStateRecord *)
{
    AV_SCOPE("Adapter::set_stable_power_state");
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_stable_power_state");
}

int32_t Adapter::control_mode_behavior(DxgkargControlModeBehaviorRecord *request)
{
    AV_SCOPE("Adapter::control_mode_behavior");
    platform_.require_irql_at_most(Irql::passive, "Adapter::control_mode_behavior");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    capabilities_.answer_mode_behavior(*request);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::update_monitor_link_info(DxgkargUpdateMonitorLinkInfoRecord *request)
{
    AV_SCOPE("Adapter::update_monitor_link_info");
    platform_.require_irql_at_most(Irql::passive, "Adapter::update_monitor_link_info");
    if (request == nullptr || request->VideoPresentTargetId >= sources_.count())
        return NtStatus(Status::invalid_parameter).value();
    edid_.describe_link(*request);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::create_native_fence(DxgkargCreateNativeFenceRecord *request)
{
    AV_SCOPE("Adapter::create_native_fence");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_native_fence");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(native_fences_.create(*request)).value();
}

int32_t Adapter::destroy_native_fence(const DxgkargDestroyNativeFenceRecord *request)
{
    AV_SCOPE("Adapter::destroy_native_fence");
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_native_fence");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(native_fences_.destroy(*request)).value();
}

int32_t Adapter::update_monitored_values(const DxgkargUpdateNativeFenceValuesRecord *request)
{
    AV_SCOPE("Adapter::update_monitored_values");
    platform_.require_irql_at_most(Irql::isr, "Adapter::update_monitored_values");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(native_fences_.update_monitored(*request)).value();
}

int32_t Adapter::update_current_values_from_cpu(const DxgkargUpdateNativeFenceValuesRecord *request)
{
    AV_SCOPE("Adapter::update_current_values_from_cpu");
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::update_current_values_from_cpu");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(native_fences_.update_current(*request)).value();
}

int32_t Adapter::create_hw_queue(void *context, DxgkargCreateHwQueueRecord *request)
{
    AV_SCOPE("Adapter::create_hw_queue");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_hw_queue");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    const Status created = hw_queues_.create(context, *request);
    if (created != Status::ok) return NtStatus(created).value();
    /* What the queue runs on, where a submission at DISPATCH can read it. */
    const Status bound = hw_queue_contexts_.bind(request->hHwQueue, static_cast<HwQueue *>(request->hHwQueue)->context());
    if (bound != Status::ok) {
        static_cast<void>(hw_queues_.destroy(request->hHwQueue));
        request->hHwQueue = nullptr;
    }
    return NtStatus(bound).value();
}

int32_t Adapter::destroy_hw_queue(void *queue)
{
    AV_SCOPE("Adapter::destroy_hw_queue");
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_hw_queue");
    /* The context writes every completion into its queue's progress while a
     * doorbell is connected, and dxgkrnl frees that fence with the queue: a
     * queue that goes before its doorbell must take the writes with it, or
     * every DPC after stores into memory Windows has handed on. */
    const HwQueue *const found = hw_queues_.find(queue);
    if (found != nullptr && av_contexts_.forget_progress(&found->context(), found->progress()) &&
        budget_.allows())
        platform_.log("avgpu: a hardware queue went before its doorbell; its context stops writing its progress");
    hw_submissions_.forget(queue);
    hw_queue_contexts_.forget(queue);
    return NtStatus(hw_queues_.destroy(queue)).value();
}

int32_t Adapter::submit_command_to_hw_queue(const DxgkargSubmitCommandToHwQueueRecord *request)
{
    AV_SCOPE("Adapter::submit_command_to_hw_queue");
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::submit_command_to_hw_queue");
    if (request == nullptr || request->hHwQueue == nullptr) return NtStatus(Status::invalid_parameter).value();
    /* What the queue runs on, read without the queue table's mutex, which a
     * submission at DISPATCH may not take (hw_queue_contexts.hpp). */
    Context *const context = hw_queue_contexts_.context_of(request->hHwQueue);
    if (context == nullptr) return NtStatus(Status::invalid_parameter).value();
    uint64_t             awaited = 0u;
    if (request->DmaBufferPrivateDataSize != 0u) {
        /* The same descriptor the virtual path carries, into the same ring:
         * a hardware-scheduled context submits this way and no other
         * (feature 049, research.md §13). */
        const Status published = av_contexts_.submit(context,
                                                      static_cast<const uint8_t *>(request->pDmaBufferPrivateData),
                                                      request->DmaBufferPrivateDataSize, awaited);
        if (published != Status::ok) return NtStatus(published).value();
    }
    const Status status = hw_submissions_.submit(request->hHwQueue, request->HwQueueProgressFenceId,
                                                 static_cast<uint64_t *>(request->HwQueueProgressFenceCpuVa),
                                                 awaited != 0u ? context : nullptr, awaited);
    if (status == Status::ok) static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(status).value();
}

/* A doorbell belongs to a hardware queue on a context, and only an av context
 * has a ring the process could publish a tail into: for any other, there is
 * nothing to hand over and the refusal is the answer. */
void Adapter::submissions_to_user_mode(const HwQueue &queue)
{
    AV_SCOPE("Adapter::submissions_to_user_mode");
    static_cast<void>(av_contexts_.user_mode_submits(&queue.context(), queue.progress()));
}

void Adapter::submissions_to_driver(void *doorbell)
{
    AV_SCOPE("Adapter::submissions_to_driver");
    const HwQueue *const queue = doorbell_table_.object().queue_of(doorbell);
    if (queue != nullptr) static_cast<void>(av_contexts_.driver_submits(&queue->context()));
}

int32_t Adapter::create_doorbell(DxgkargCreateDoorbellRecord *request)
{
    AV_SCOPE("Adapter::create_doorbell");
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_doorbell");
    if (request == nullptr || request->PrivateDriverDataSize != 0u || request->Flags != 0u)
        return NtStatus(Status::invalid_parameter).value();
    if (!doorbell_table_.constructed()) return NtStatus(Status::not_found).value();
    /* The page is the one the queue's context was bound to: a doorbell names
     * the context's own ring, and the device wakes that context's worker. */
    HwQueue *const queue = hw_queues_.find(request->hHwQueue);
    if (queue == nullptr) return NtStatus(Status::invalid_parameter).value();
    const uint32_t page = av_contexts_.doorbell_of(&queue->context());
    void          *made = nullptr;
    const Status   created = doorbell_table_.object().create(*queue, page, made);
    if (created != Status::ok) return NtStatus(created).value();
    request->hDoorbell = made;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::connect_doorbell(DxgkargConnectDoorbellRecord *request)
{
    AV_SCOPE("Adapter::connect_doorbell");
    platform_.require_irql_at_most(Irql::passive, "Adapter::connect_doorbell");
    if (request == nullptr || request->Flags != 0u) return NtStatus(Status::invalid_parameter).value();
    if (!doorbell_table_.constructed()) return NtStatus(Status::not_found).value();
    void          *address = nullptr;
    DoorbellStatus status = DoorbellStatus::disconnected_abort;
    HwQueue       *queue = nullptr;
    const Status   connected = doorbell_table_.object().connect(request->hDoorbell, address, status, queue);
    if (connected != Status::ok) return NtStatus(connected).value();
    request->KernelCpuVirtualAddress = address;
    /* No secondary doorbell: the caps say so, and a process that asked for one
     * anyway is answered the one page there is. */
    request->SecondaryKernelCpuVirtualAddress = nullptr;
    request->Status = static_cast<uint32_t>(status);
    /* From here the process publishes the tail and rings the page itself, and
     * neither is the driver's to judge until the doorbell goes away. */
    submissions_to_user_mode(*queue);
    return NtStatus(Status::ok).value();
}

int32_t Adapter::disconnect_doorbell(const DxgkargDisconnectDoorbellRecord *request)
{
    AV_SCOPE("Adapter::disconnect_doorbell");
    platform_.require_irql_at_most(Irql::passive, "Adapter::disconnect_doorbell");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (!doorbell_table_.constructed()) return NtStatus(Status::not_found).value();
    /* Before the disconnect, while the doorbell still names its queue: a
     * process that stores after this has submitted nothing, and the fallback
     * it makes instead goes through the driver again. */
    submissions_to_driver(request->hDoorbell);
    return NtStatus(doorbell_table_.object().disconnect(request->hDoorbell)).value();
}

int32_t Adapter::destroy_doorbell(const DxgkargDestroyDoorbellRecord *request)
{
    AV_SCOPE("Adapter::destroy_doorbell");
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_doorbell");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (!doorbell_table_.constructed()) return NtStatus(Status::not_found).value();
    submissions_to_driver(request->hDoorbell);
    return NtStatus(doorbell_table_.object().destroy(request->hDoorbell)).value();
}

int32_t Adapter::notify_work_submission(const DxgkargNotifyWorkSubmissionRecord *request)
{
    AV_SCOPE("Adapter::notify_work_submission");
    platform_.require_irql_at_most(Irql::passive, "Adapter::notify_work_submission");
    if (request == nullptr || hw_queues_.find(request->hHwQueue) == nullptr)
        return NtStatus(Status::invalid_parameter).value();
    /* Nothing to do: the work is in the ring and the store already woke the
     * worker. A driver whose doorbell needed a kernel push would push here. */
    return NtStatus(Status::ok).value();
}

int32_t Adapter::reset_hw_engine(const DxgkargResetHwEngineRecord *request)
{
    AV_SCOPE("Adapter::reset_hw_engine");
    platform_.require_irql_at_most(Irql::passive, "Adapter::reset_hw_engine");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    hw_submissions_.abandon();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::resume_hw_engine(const DxgkargResumeHwEngineRecord *request)
{
    AV_SCOPE("Adapter::resume_hw_engine");
    platform_.require_irql_at_most(Irql::passive, "Adapter::resume_hw_engine");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    return NtStatus(Status::ok).value();
}

void Adapter::report(StateWriter &writer)
{
    AV_SCOPE("Adapter::report");
    platform_.require_irql_at_most(Irql::passive, "Adapter::report");
    writer.open("adapter", 0u);
    writer.flag("started", started_);
    writer.flag("present", present_);
    writer.flag("powering_down", transition_.held());
    writer.number("presented_fence", presented_fence_);
    writer.number("present_rows", present_rows_);
    writer.number("presents_unmoved", presents_unmoved_);
    writer.number("presents_unticketed", presents_unticketed_);
    writer.hex("display_changed", display_changed_);
    writer.hex("aperture_bytes", segment_.size());
    writer.flag("host_visible", host_visible_.offered());
    writer.hex("host_visible_base", host_visible_.base());
    writer.hex("host_visible_bytes", host_visible_.size());
    writer.number("vsync_enables", vsync_.enabled_count());
    writer.number("vsync_reported", vsync_.reported());
    devices_.report(writer);
    allocations_.report(writer);
    shared_.report(writer);
    contexts_.report(writer);
    hw_queues_.report(writer);
    node_.report(writer);
    av_contexts_.report(writer);
    sources_.report(writer);
    writer.close();
}

bool Adapter::invariants_hold() const
{
    AV_SCOPE("Adapter::invariants_hold");
    const bool powered_only_when_started = !device_.constructed() || started_;
    const bool mapped_only_when_started = mappings_.mapped_bars() == 0u || started_;
    const bool presents_with_the_device =
        !presents_.constructed() || (device_.constructed() && presents_.object().invariants_hold());
    return powered_only_when_started && mapped_only_when_started && presents_with_the_device;
}

}  // namespace av
