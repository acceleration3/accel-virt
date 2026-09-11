#include "dxgkrnl_interface.hpp"

#include "invariant_scope.hpp"
#include "nt_status.hpp"

namespace av {

DxgkrnlInterface::DxgkrnlInterface(Platform &platform)
    : platform_(platform), record_(), initialized_(false) {}

bool DxgkrnlInterface::has_callbacks(const DxgkrnlInterfaceRecord &table) const
{
    return table.DxgkCbGetDeviceInformation != nullptr && table.DxgkCbMapMemory != nullptr &&
           table.DxgkCbQueueDpc != nullptr && table.DxgkCbReadDeviceSpace != nullptr &&
           table.DxgkCbUnmapMemory != nullptr;
}

Status DxgkrnlInterface::initialize(const DxgkrnlInterfaceRecord *table)
{
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
    platform_.require_irql_at_most(Irql::passive, "DxgkrnlInterface::get_device_information");
    if (!initialized_) return Status::invalid_state;
    return NtStatus(static_cast<uint32_t>(
                        record_.DxgkCbGetDeviceInformation(record_.DeviceHandle, &info)))
        .status();
}

Status DxgkrnlInterface::read_config_space(uint8_t *buffer, uint32_t length)
{
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
    platform_.require_irql_at_most(Irql::passive, "DxgkrnlInterface::unmap_memory");
    if (!initialized_) return Status::invalid_state;
    return NtStatus(static_cast<uint32_t>(record_.DxgkCbUnmapMemory(record_.DeviceHandle, mapped)))
        .status();
}

bool DxgkrnlInterface::queue_dpc()
{
    platform_.require_irql_at_most(Irql::isr, "DxgkrnlInterface::queue_dpc");
    if (!initialized_) return false;
    return record_.DxgkCbQueueDpc(record_.DeviceHandle) != 0u;
}

bool DxgkrnlInterface::initialized() const { return initialized_; }

bool DxgkrnlInterface::invariants_hold() const { return !initialized_ || has_callbacks(record_); }

}  // namespace av
