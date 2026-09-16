#include "core/scheduling/device_table.hpp"

#include "core/ddi/dxgk_open_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/miniport/acquired_handle_data.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

DeviceTable::DeviceTable(Platform &platform, AllocationTable &allocations, DxgkrnlInterface &dxgkrnl)
    : platform_(platform), allocations_(allocations), dxgkrnl_(dxgkrnl), mutex_(platform, LockRank::device_table),
      devices_(platform), opens_(platform)
      {
          AV_SCOPE("DeviceTable::DeviceTable");
      }

Status DeviceTable::create(Adapter &adapter, DxgkargCreateDeviceRecord &request)
{
    AV_SCOPE("DeviceTable::create");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<DeviceTable> guard(platform_, *this, "create");
    Device *device = nullptr;
    const Status status = devices_.create(device, platform_, adapter, request.hDevice, request.Flags);
    if (status == Status::ok) request.hDevice = device;
    return status;
}

Status DeviceTable::destroy(void *device)
{
    AV_SCOPE("DeviceTable::destroy");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::destroy");
    MutexGuard lock(mutex_);
    InvariantScope<DeviceTable> guard(platform_, *this, "destroy");
    Device *target = devices_.find(device);
    if (target == nullptr) return Status::invalid_parameter;
    opens_.destroy_if([&](const DeviceAllocation &open) {
        if (&open.device() != target) return false;
        static_cast<void>(allocations_.close(open.allocation()));
        return true;
    });
    return devices_.destroy(target);
}

/* Each handle is resolved before the mutex raises to APC, and the borrowed data
 * is released as each AcquiredHandleData goes out of scope - the pointer it
 * answered is this driver's own allocation, which outlives the loan.
 * hDeviceSpecificAllocation holds it until the open replaces it. */
Status DeviceTable::resolve(DxgkargOpenAllocationRecord &request)
{
    AV_SCOPE("DeviceTable::resolve");
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        DxgkOpenAllocationInfoRecord &info = request.pOpenAllocation[index];
        AcquiredHandleData acquired(dxgkrnl_, info.hAllocation);
        const Status       status = acquired.acquire();
        info.hDeviceSpecificAllocation = acquired.data();
        if (status != Status::ok) return status;
    }
    return Status::ok;
}

Status DeviceTable::open_one(Device &device, DxgkOpenAllocationInfoRecord &info)
{
    AV_SCOPE("DeviceTable::open_one");
    Allocation *allocation = allocations_.find(info.hDeviceSpecificAllocation);
    info.hDeviceSpecificAllocation = nullptr;
    if (allocation == nullptr) return Status::invalid_parameter;
    DeviceAllocation *open = nullptr;
    const Status created = opens_.create(open, device, *allocation);
    if (created != Status::ok) return created;
    info.hDeviceSpecificAllocation = open;
    return allocations_.open(*allocation, &device);
}

/* The first `count` entries hold opens; the rest may still hold resolved
 * handle data, which is not an answer either. */
void DeviceTable::close_opened(DxgkargOpenAllocationRecord &request, uint32_t count)
{
    AV_SCOPE("DeviceTable::close_opened");
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        DxgkOpenAllocationInfoRecord &info = request.pOpenAllocation[index];
        auto *open = static_cast<DeviceAllocation *>(info.hDeviceSpecificAllocation);
        if (index < count) {
            static_cast<void>(allocations_.close(open->allocation()));
            static_cast<void>(opens_.destroy(open));
        }
        info.hDeviceSpecificAllocation = nullptr;
    }
}

Status DeviceTable::open(void *device, DxgkargOpenAllocationRecord &request)
{
    AV_SCOPE("DeviceTable::open");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::open");
    if (request.NumAllocations == 0u || request.NumAllocations > AllocationTable::max_per_call ||
        request.pOpenAllocation == nullptr)
        return Status::invalid_parameter;
    uint32_t opened = 0u;
    ScopeExit unwind([&] { close_opened(request, opened); });
    Status status = resolve(request);
    if (status != Status::ok) return status;
    MutexGuard lock(mutex_);
    InvariantScope<DeviceTable> guard(platform_, *this, "open");
    Device *target = devices_.find(device);
    if (target == nullptr) return Status::invalid_parameter;
    for (; opened < request.NumAllocations; opened++) {
        status = open_one(*target, request.pOpenAllocation[opened]);
        if (status != Status::ok) return status;
    }
    unwind.dismiss();
    const auto *first = static_cast<DeviceAllocation *>(request.pOpenAllocation[0].hDeviceSpecificAllocation);
    request.SubresourceOffset = 0u;
    request.Pitch = first->allocation().descriptor().layout().pitch();
    return Status::ok;
}

Status DeviceTable::close(void *device, const DxgkargCloseAllocationRecord &request)
{
    AV_SCOPE("DeviceTable::close");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::close");
    MutexGuard lock(mutex_);
    InvariantScope<DeviceTable> guard(platform_, *this, "close");
    const Device *target = devices_.find(device);
    if (target == nullptr || (request.NumAllocations != 0u && request.pOpenHandleList == nullptr))
        return Status::invalid_parameter;
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        const DeviceAllocation *open = opens_.find(request.pOpenHandleList[index]);
        if (open == nullptr || &open->device() != target) return Status::invalid_parameter;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (request.pOpenHandleList[earlier] == request.pOpenHandleList[index]) return Status::invalid_parameter;
    }
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        auto *open = static_cast<DeviceAllocation *>(request.pOpenHandleList[index]);
        static_cast<void>(allocations_.close(open->allocation()));
        static_cast<void>(opens_.destroy(open));
    }
    return Status::ok;
}

Device *DeviceTable::find(void *device)
{
    AV_SCOPE("DeviceTable::find");
    platform_.require_irql_at_most(Irql::apc, "DeviceTable::find");
    MutexGuard lock(mutex_);
    return devices_.find(device);
}

uint32_t DeviceTable::devices() const
{
    AV_SCOPE("DeviceTable::devices");
    return devices_.live();
}
uint32_t DeviceTable::opens() const
{
    AV_SCOPE("DeviceTable::opens");
    return opens_.live();
}

/* Every open joins a live device to an allocation that counts it. */
void DeviceTable::report(StateWriter &writer)
{
    AV_SCOPE("DeviceTable::report");
    platform_.require_irql_at_most(Irql::apc, "DeviceTable::report");
    const MutexTryGuard guard(mutex_);
    writer.open("device_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("devices", devices_.live());
        writer.number("opens", opens_.live());
        uint64_t index = 0u;
        devices_.for_each([&](const Device &device) {
            writer.open("device", index++);
            writer.address("handle", &device);
            writer.address("runtime", device.runtime_handle());
            writer.hex("flags", device.flags());
            uint64_t open = 0u;
            opens_.for_each([&](const DeviceAllocation &opened) {
                if (&opened.device() != &device) return;
                writer.open("open", open++);
                writer.address("allocation", &opened.allocation());
                writer.close();
            });
            writer.close();
        });
    }
    writer.close();
}

bool DeviceTable::invariants_hold() const
{
    AV_SCOPE("DeviceTable::invariants_hold");
    const bool pools = devices_.invariants_hold() && opens_.invariants_hold();
    return pools && opens_.all_of([this](const DeviceAllocation &open) {
               return devices_.find(&open.device()) != nullptr && open.allocation().opens() != 0u;
           });
}

}  // namespace av
