#include "core/miniport/dxgkrnl_interface.hpp"

#include "core/support/invariant_scope.hpp"
#include "core/support/nt_status.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

DxgkrnlInterface::DxgkrnlInterface(Platform &platform)
    : platform_(platform), record_(), initialized_(false)
    {
        AV_SCOPE("DxgkrnlInterface::DxgkrnlInterface");
    }

bool DxgkrnlInterface::has_callbacks(const DxgkrnlInterfaceRecord &table) const
{
    AV_SCOPE("DxgkrnlInterface::has_callbacks");
    return table.DxgkCbGetDeviceInformation != nullptr && table.DxgkCbMapMemory != nullptr &&
           table.DxgkCbQueueDpc != nullptr && table.DxgkCbReadDeviceSpace != nullptr &&
           table.DxgkCbUnmapMemory != nullptr && table.DxgkCbAcquireHandleData != nullptr &&
           table.DxgkCbReleaseHandleData != nullptr &&
           table.DxgkCbSynchronizeExecution != nullptr && table.DxgkCbNotifyInterrupt != nullptr &&
           table.DxgkCbNotifyDpc != nullptr && table.DxgkCbQueryVidPnInterface != nullptr;
}

Status DxgkrnlInterface::initialize(const DxgkrnlInterfaceRecord *table)
{
    AV_SCOPE("DxgkrnlInterface::initialize");
    platform_.require_irql_at_most(Irql::passive, "DxgkrnlInterface::initialize");
    InvariantScope<DxgkrnlInterface> guard(platform_, *this, "initialize");
    initialized_ = false;
    if (table == nullptr) return Status::invalid_parameter;
    if (table->Size < sizeof record_) return Status::not_supported;
    if (!has_callbacks(*table)) return Status::invalid_parameter;
    platform_.copy(&record_, table, sizeof record_);
    initialized_ = true;
    return Status::ok;
}

Status DxgkrnlInterface::get_device_information(DxgkDeviceInfoRecord &info)
{
    AV_SCOPE("DxgkrnlInterface::get_device_information");
    platform_.require_irql_at_most(Irql::passive, "DxgkrnlInterface::get_device_information");
    if (!initialized_) return Status::invalid_state;
    return NtStatus(static_cast<uint32_t>(
                        record_.DxgkCbGetDeviceInformation(record_.DeviceHandle, &info)))
        .status();
}

Status DxgkrnlInterface::read_config_space(uint8_t *buffer, uint32_t length)
{
    AV_SCOPE("DxgkrnlInterface::read_config_space");
    platform_.require_irql_at_most(Irql::passive, "DxgkrnlInterface::read_config_space");
    if (!initialized_) return Status::invalid_state;
    uint32_t read = 0u;
    const Status status = NtStatus(static_cast<uint32_t>(record_.DxgkCbReadDeviceSpace(
                                       record_.DeviceHandle, config_space, buffer, 0u, length, &read)))
                              .status();
    if (status != Status::ok) return status;
    return read == length ? Status::ok : Status::malformed;
}

Status DxgkrnlInterface::map_memory(uint64_t translated_address, uint32_t length, void *&mapped)
{
    AV_SCOPE("DxgkrnlInterface::map_memory");
    platform_.require_irql_at_most(Irql::passive, "DxgkrnlInterface::map_memory");
    mapped = nullptr;
    if (!initialized_) return Status::invalid_state;
    void *address = nullptr;
    const Status status = NtStatus(static_cast<uint32_t>(record_.DxgkCbMapMemory(
                                       record_.DeviceHandle, static_cast<int64_t>(translated_address),
                                       length, 0u, 0u, non_cached, &address)))
                              .status();
    if (status == Status::ok) mapped = address;
    return status;
}

Status DxgkrnlInterface::unmap_memory(void *mapped)
{
    AV_SCOPE("DxgkrnlInterface::unmap_memory");
    platform_.require_irql_at_most(Irql::passive, "DxgkrnlInterface::unmap_memory");
    if (!initialized_) return Status::invalid_state;
    return NtStatus(static_cast<uint32_t>(record_.DxgkCbUnmapMemory(record_.DeviceHandle, mapped)))
        .status();
}

bool DxgkrnlInterface::queue_dpc()
{
    AV_SCOPE("DxgkrnlInterface::queue_dpc");
    platform_.require_irql_at_most(Irql::isr, "DxgkrnlInterface::queue_dpc");
    if (!initialized_) return false;
    return record_.DxgkCbQueueDpc(record_.DeviceHandle) != 0u;
}

Status DxgkrnlInterface::acquire_handle_data(uint32_t allocation_handle, void *&data, void *&release_handle)
{
    AV_SCOPE("DxgkrnlInterface::acquire_handle_data");
    platform_.require_irql_at_most(Irql::apc, "DxgkrnlInterface::acquire_handle_data");
    data = nullptr;
    release_handle = nullptr;
    if (!initialized_) return Status::invalid_state;
    DxgkargcbGetHandleDataRecord arguments = {};
    arguments.hObject = allocation_handle;
    arguments.Type = handle_allocation;
    data = record_.DxgkCbAcquireHandleData(&arguments, &release_handle);
    return data != nullptr ? Status::ok : Status::invalid_parameter;
}

void DxgkrnlInterface::release_handle_data(void *release_handle)
{
    AV_SCOPE("DxgkrnlInterface::release_handle_data");
    platform_.require_irql_at_most(Irql::apc, "DxgkrnlInterface::release_handle_data");
    DxgkargcbReleaseHandleDataRecord arguments = {};
    arguments.ReleaseHandle = release_handle;
    arguments.Type = handle_allocation;
    record_.DxgkCbReleaseHandleData(arguments);
}

Status DxgkrnlInterface::synchronize_execution(SynchronizeRoutine routine, void *context)
{
    AV_SCOPE("DxgkrnlInterface::synchronize_execution");
    platform_.require_irql_at_most(Irql::dispatch, "DxgkrnlInterface::synchronize_execution");
    if (!initialized_) return Status::invalid_state;
    uint8_t returned = 0u;
    return NtStatus(static_cast<uint32_t>(
                        record_.DxgkCbSynchronizeExecution(record_.DeviceHandle, routine, context, 0u, &returned)))
        .status();
}

void DxgkrnlInterface::notify_interrupt(const DxgkargcbNotifyInterruptDataRecord &data)
{
    AV_SCOPE("DxgkrnlInterface::notify_interrupt");
    platform_.require_irql_at_most(Irql::isr, "DxgkrnlInterface::notify_interrupt");
    if (initialized_) record_.DxgkCbNotifyInterrupt(record_.DeviceHandle, &data);
}

void DxgkrnlInterface::notify_dpc()
{
    AV_SCOPE("DxgkrnlInterface::notify_dpc");
    platform_.require_irql_at_most(Irql::dispatch, "DxgkrnlInterface::notify_dpc");
    if (initialized_) record_.DxgkCbNotifyDpc(record_.DeviceHandle);
}

Status DxgkrnlInterface::query_vidpn_interface(void *vidpn, const DxgkVidPnInterfaceRecord *&vidpn_interface)
{
    AV_SCOPE("DxgkrnlInterface::query_vidpn_interface");
    platform_.require_irql_at_most(Irql::apc, "DxgkrnlInterface::query_vidpn_interface");
    vidpn_interface = nullptr;
    if (!initialized_) return Status::invalid_state;
    const Status status = NtStatus(static_cast<uint32_t>(
                                       record_.DxgkCbQueryVidPnInterface(vidpn, vidpn_interface_v1, &vidpn_interface)))
                              .status();
    if (status == Status::ok && vidpn_interface == nullptr) return Status::malformed;
    return status;
}

bool DxgkrnlInterface::initialized() const
{
    AV_SCOPE("DxgkrnlInterface::initialized");
    return initialized_;
}

void *DxgkrnlInterface::device_handle() const
{
    AV_SCOPE("DxgkrnlInterface::device_handle");
    platform_.require_irql_at_most(Irql::isr, "DxgkrnlInterface::device_handle");
    return initialized_ ? record_.DeviceHandle : nullptr;
}

bool DxgkrnlInterface::invariants_hold() const
{
    AV_SCOPE("DxgkrnlInterface::invariants_hold");
    return !initialized_ || has_callbacks(record_);
}

}  // namespace av
