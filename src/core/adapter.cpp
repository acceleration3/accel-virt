#include "adapter.hpp"

#include "ddi/dxgk_child_descriptor_record.hpp"
#include "ddi/dxgk_child_status_record.hpp"
#include "adapter_info_query.hpp"
#include "ddi/dxgk_device_info_record.hpp"
#include "ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "ddi/dxgkarg_escape_record.hpp"
#include "ddi/dxgkarg_query_adapter_info_record.hpp"
#include "debug_report.hpp"
#include "ddi/linked_device_record.hpp"
#include "invariant_scope.hpp"
#include "nt_status.hpp"
#include "pci_base_addresses.hpp"
#include "scope_exit.hpp"
#include "virtio_capability_walk.hpp"

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
      resources_(platform), mappings_(platform, dxgkrnl_), layout_(), device_(), capabilities_(platform), started_(false) {}

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
    return status;
}

Status Adapter::power_down()
{
    if (!device_.constructed()) return Status::ok;
    const Status status = device_.object().stop();
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
    ScopeExit unmap([this] { mappings_.unmap_all(); });
    Status status = discover_and_map(dxgkrnl_interface);
    if (status == Status::ok) status = power_up();
    if (status != Status::ok) return NtStatus(status).value();
    unmap.dismiss();
    *sources = 1u;
    *children = 1u;
    started_ = true;
    return NtStatus(Status::ok).value();
}

int32_t Adapter::stop_device()
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_device");
    InvariantScope<Adapter> guard(platform_, *this, "stop_device");
    const Status status = power_down();
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
    if (!device_.constructed()) return 0u;
    if (resources_.message_interrupts() == 0u && device_.object().read_isr() == 0u) return 0u;
    static_cast<void>(dxgkrnl_.queue_dpc());
    return 1u;
}

void Adapter::dpc_routine()
{
    platform_.require_irql_at_most(Irql::dispatch, "Adapter::dpc_routine");
    if (!device_.constructed()) return;
    uint32_t completed = 0u;
    if (device_.object().harvest_control(completed) != Status::ok)
        platform_.log("rvkgpu: control queue harvest failed in the DPC");
}

int32_t Adapter::query_child_relations(DxgkChildDescriptorRecord *relations, uint32_t size)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_relations");
    if (relations == nullptr) return NtStatus(Status::invalid_parameter).value();
    if (size < sizeof *relations) return NtStatus(NtStatus::buffer_too_small).value();
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

int32_t Adapter::query_device_descriptor(uint32_t, void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_device_descriptor");
    return NtStatus(NtStatus::child_descriptor_not_supported).value();
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
    switch (request.type()) {
    case query_driver_caps:
        return NtStatus(capabilities_.report_driver_caps(request)).value();
    case query_wddm_device_caps:
        return NtStatus(capabilities_.report_wddm_device_caps(request)).value();
    default:
        return NtStatus(NtStatus::not_implemented).value();
    }
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
    const DebugReport report(platform_, started_, powered, powered ? device_.object().features() : 0u);
    static_cast<void>(report.write(collect->pBuffer, collect->BufferSize));
    return NtStatus(Status::ok).value();
}

bool Adapter::invariants_hold() const
{
    const bool powered_only_when_started = !device_.constructed() || started_;
    const bool mapped_only_when_started = mappings_.mapped_bars() == 0u || started_;
    return powered_only_when_started && mapped_only_when_started;
}

}  // namespace av
