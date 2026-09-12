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
#include "core/ddi/dxgkarg_create_process_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_escape_record.hpp"
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
#include "core/scheduling/patch_request.hpp"
#include "core/scheduling/render_request.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/text_buffer.hpp"
#include "core/support/scope_exit.hpp"
#include "core/vidpn/cofunc_modality.hpp"
#include "core/vidpn/vidpn_commit.hpp"
#include "core/vidpn/vidpn_support_check.hpp"
#include "core/virtio/virtio_capability_walk.hpp"

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
    : platform_(platform), physical_device_object_(physical_device_object), dxgkrnl_(platform),
      resources_(platform), mappings_(platform, dxgkrnl_), layout_(), device_(), capabilities_(platform), geometry_(platform),
      segment_(platform),
      modes_(), edid_(platform, modes_.preferred()), sources_(platform),
      allocations_(platform, segment_), devices_(platform, allocations_, dxgkrnl_), contexts_(platform, devices_, geometry_),
      hw_queues_(platform, contexts_), hw_submissions_(platform), node_(platform), notification_(platform, dxgkrnl_), vsync_(platform, dxgkrnl_, sources_),
      cursor_(platform, device_), budget_(platform), started_(false), present_(true) {}

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
    return status;
}

Status Adapter::power_up()
{
    /* The only refusal is storage already holding a device: already powered. */
    if (device_.construct(platform_, mappings_, layout_) != Status::ok) return Status::ok;
    const Status status = device_.object().start(resources_.message_interrupts());
    if (status != Status::ok) device_.destroy();
    cursor_.forget_device();
    return status;
}

/* A removed device cannot acknowledge a reset; destruction still abandons it. */
Status Adapter::power_down()
{
    if (!device_.constructed()) return Status::ok;
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
        vsync_.destroy();
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
    *sources = DisplaySources::source_count;
    *children = DisplaySources::target_count;
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
    cursor_.release();
    mappings_.unmap_all();
    started_ = false;
    return NtStatus(status).value();
}

int32_t Adapter::dispatch_io_request(uint32_t, void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::dispatch_io_request");
    return NtStatus(Status::not_supported).value();
}

/* A message interrupt is ours by construction; a line interrupt only if the
 * device set an ISR bit, since the line may be shared. */
uint8_t Adapter::interrupt_routine(uint32_t)
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::interrupt_routine");
    if (!device_.constructed() || !present_) return 0u;
    if (resources_.message_interrupts() == 0u && device_.object().read_isr() == 0u) return 0u;
    static_cast<void>(dxgkrnl_.queue_dpc());
    return 1u;
}

void Adapter::dpc_routine()
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::dpc_routine");
    uint32_t completed = 0u;
    const bool harvest = device_.constructed() && present_;
    /* A device that fails every harvest would otherwise log on every DPC, so
     * each of these lines is spent from the budget (feature 037, FR-003). */
    if (harvest && device_.object().harvest_control(completed) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: control queue harvest failed in the DPC");
    if (harvest && device_.object().harvest_cursor(completed) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: cursor queue harvest failed in the DPC");
    if (node_.complete(notification_) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: a completion was refused in the DPC");
    if (hw_submissions_.complete(notification_) != Status::ok && budget_.allows())
        platform_.log("rvkgpu: a progress fence signal was refused in the DPC");
    dxgkrnl_.notify_dpc();
}

int32_t Adapter::query_child_relations(DxgkChildDescriptorRecord *relations, uint32_t size)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_relations");
    if (relations == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (size < sizeof *relations) return NtStatus(NtStatus::buffer_too_small).value();
    platform_.log("rvkgpu: QueryChildRelations");
    relations->ChildDeviceType = type_video_output;
    relations->InterfaceTechnology = output_technology_other;
    relations->MonitorOrientationAwareness = orientation_awareness_none;
    relations->SupportsSdtvModes = 0u;
    relations->HpdAwareness = hpd_interruptible;
    relations->AcpiUid = 0u;
    relations->ChildUid = 0u;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_child_status(DxgkChildStatusRecord *status, uint8_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_status");
    if (status == nullptr || status->ChildUid != 0u) return NtStatus(Status::invalid_parameter).value();
    if (status->Type == status_rotation) return NtStatus(Status::invalid_parameter).value();
    if (status->Type != status_connection) return NtStatus(Status::not_supported).value();
    status->HotPlugConnected = 1u;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::query_device_descriptor(uint32_t child, DxgkDeviceDescriptorRecord *descriptor)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_device_descriptor");
    if (child != 0u || descriptor == nullptr) return NtStatus(Status::invalid_parameter).value();
    return edid_.copy(*descriptor).value();
}

int32_t Adapter::set_power_state(uint32_t device, int32_t power_state, int32_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_power_state");
    InvariantScope<Adapter> guard(platform_, *this, "set_power_state");
    if (device != adapter_device_uid) return NtStatus(Status::ok).value();
    if (!started_) return NtStatus(Status::invalid_state).value();
    if (power_state == power_d0) return NtStatus(power_up()).value();
    if (power_state > power_d0 && power_state <= power_d3) return NtStatus(power_down()).value();
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
        answered = NtStatus(segment_.report(request)).value();
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
    return NtStatus(Status::not_supported).value();
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
    return NtStatus(devices_.create(*this, *request)).value();
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
    return NtStatus(allocations_.create(*request)).value();
}

int32_t Adapter::destroy_allocation(const DxgkargDestroyAllocationRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(allocations_.destroy(*request)).value();
}

int32_t Adapter::describe_allocation(DxgkargDescribeAllocationRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::describe_allocation");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(allocations_.describe(*request, sources_)).value();
}

int32_t Adapter::get_standard_allocation_driver_data(DxgkargGetStandardAllocationDriverDataRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_standard_allocation_driver_data");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(StandardAllocation(platform_, *request).describe()).value();
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
    return NtStatus(sources_.set_private_format(request->VidPnSourceId, request->PrivateDriverFormatAttribute)).value();
}

int32_t Adapter::create_context(void *device, DxgkargCreateContextRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::create_context");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(contexts_.create(device, *request)).value();
}

int32_t Adapter::destroy_context(void *context)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::destroy_context");
    /* DXGCONTEXT::DestroyContext destroys a context's queues first. */
    if (hw_queues_.serves(context)) return NtStatus(Status::invalid_parameter).value();
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
    return PagingRequest(platform_, *request).build().value();
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
    if (status == Status::ok) node_.restart();
    return NtStatus(status).value();
}

int32_t Adapter::is_supported_vidpn(DxgkargIsSupportedVidPnRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::is_supported_vidpn");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return VidPnSupportCheck(platform_, dxgkrnl_, *request).decide().value();
}

int32_t Adapter::recommend_functional_vidpn(const DxgkargRecommendFunctionalVidPnRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_functional_vidpn");
    return NtStatus(NtStatus::no_recommended_functional_vidpn).value();
}

int32_t Adapter::enum_vidpn_cofunc_modality(const DxgkargEnumVidPnCofuncModalityRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::enum_vidpn_cofunc_modality");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return CofuncModality(platform_, dxgkrnl_, modes_, *request).enumerate().value();
}

int32_t Adapter::commit_vidpn(const DxgkargCommitVidPnRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::commit_vidpn");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return VidPnCommit(platform_, dxgkrnl_, modes_, sources_, *request).apply().value();
}

int32_t Adapter::update_active_vidpn_present_path(const D3dkmdtVidPnPresentPathRecord *path)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::update_active_vidpn_present_path");
    if (path == nullptr || path->VidPnSourceId >= DisplaySources::source_count ||
        path->VidPnTargetId >= DisplaySources::target_count)
        return NtStatus(Status::invalid_parameter).value();
    if (path->ContentTransformationScaling != transformation_identity ||
        path->ContentTransformationRotation != transformation_identity)
        return NtStatus(NtStatus::path_content_geometry_transformation_not_supported).value();
    return NtStatus(Status::ok).value();
}

int32_t Adapter::recommend_monitor_modes(const DxgkargRecommendMonitorModesRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_monitor_modes");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return MonitorModes(platform_, modes_, *request).recommend().value();
}

int32_t Adapter::recommend_vidpn_topology(const DxgkargRecommendVidPnTopologyRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::recommend_vidpn_topology");
    return NtStatus(NtStatus::no_recommended_vidpn_topology).value();
}

/* Target 0 shows source 0. */
int32_t Adapter::get_scan_line(DxgkargGetScanLineRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_scan_line");
    if (request == nullptr || request->VidPnTargetId >= DisplaySources::target_count)
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
    if (request->hAllocation == nullptr) return NtStatus(sources_.set_scanout(request->VidPnSourceId, 0)).value();
    if (request->PrimarySegment != ApertureSegment::segment_id && request->PrimarySegment != system_memory_driver_segment)
        return NtStatus(Status::invalid_parameter).value();
    return NtStatus(sources_.set_scanout(request->VidPnSourceId, request->PrimaryAddress)).value();
}

int32_t Adapter::set_vidpn_source_visibility(const DxgkargSetVidPnSourceVisibilityRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_vidpn_source_visibility");
    if (request == nullptr) return NtStatus(Status::invalid_parameter).value();
    return NtStatus(sources_.set_visible(request->VidPnSourceId, request->Visible != 0u)).value();
}

int32_t Adapter::present(void *context, DxgkargPresentRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::present");
    if (request == nullptr || contexts_.find(context) == nullptr) return NtStatus(Status::invalid_parameter).value();
    return PresentRequest(platform_, *request).present().value();
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
        request->SourceId >= DisplaySources::source_count || request->TargetId >= DisplaySources::target_count)
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
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_engine_status");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
    request->EngineStatus |= engine_responsive;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::reset_engine(DxgkargResetEngineRecord *request)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::reset_engine");
    if (request == nullptr || request->NodeOrdinal != 0u || request->EngineOrdinal != 0u)
        return NtStatus(Status::invalid_parameter).value();
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
    if (display == nullptr || target >= DisplaySources::target_count) return NtStatus(Status::invalid_parameter).value();
    *display = DxgkDisplayInformationRecord{};
    return stop_device();
}

int32_t Adapter::get_child_container_id(uint32_t child, DxgkChildContainerIdRecord *container)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::get_child_container_id");
    if (container == nullptr || child >= DisplaySources::target_count) return NtStatus(Status::invalid_parameter).value();
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
    const Status status = node_.submit(request->SubmissionFenceId);
    if (status == Status::ok) static_cast<void>(dxgkrnl_.queue_dpc());
    return NtStatus(status).value();
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
    if (request == nullptr || request->VideoPresentTargetId >= DisplaySources::target_count)
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
