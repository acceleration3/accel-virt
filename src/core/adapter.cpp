#include "adapter.hpp"

#include "nt_status.hpp"

namespace av {

Adapter::Adapter(Platform &platform, void *physical_device_object)
    : platform_(platform), physical_device_object_(physical_device_object) {}

void *Adapter::physical_device_object() const
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::physical_device_object");
    return physical_device_object_;
}

int32_t Adapter::start_device(void *, void *, uint32_t *, uint32_t *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::start_device");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::stop_device()
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::stop_device");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::dispatch_io_request(uint32_t, void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::dispatch_io_request");
    return NtStatus(Status::not_supported).value();
}

uint8_t Adapter::interrupt_routine(uint32_t)
{
    platform_.require_irql_at_most(Irql::isr, "Adapter::interrupt_routine");
    return 0u;
}

void Adapter::dpc_routine() { platform_.require_irql_at_most(Irql::dispatch, "Adapter::dpc_routine"); }

int32_t Adapter::query_child_relations(void *, uint32_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_relations");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::query_child_status(void *, uint8_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_child_status");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::query_device_descriptor(uint32_t, void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_device_descriptor");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::set_power_state(uint32_t, int32_t, int32_t)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::set_power_state");
    return NtStatus(Status::not_supported).value();
}

/* dxgkrnl forwards ACPI events it does not handle itself; nothing to do. */
int32_t Adapter::notify_acpi_event(int32_t, uint32_t, void *, uint32_t *acpi_flags)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::notify_acpi_event");
    *acpi_flags = 0u;
    return NtStatus(Status::ok).value();
}

void Adapter::reset_device() { platform_.require_irql_at_most(Irql::isr, "Adapter::reset_device"); }

int32_t Adapter::query_interface(void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::query_interface");
    return NtStatus(Status::not_supported).value();
}

int32_t Adapter::link_device(void *, void *)
{
    platform_.require_irql_at_most(Irql::passive, "Adapter::link_device");
    return NtStatus(Status::not_supported).value();
}

}  // namespace av
