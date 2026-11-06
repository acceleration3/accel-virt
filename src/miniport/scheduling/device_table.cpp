#include "miniport/scheduling/device_table.hpp"

#include "miniport/ddi/dxgk_open_allocation_info_record.hpp"
#include "miniport/ddi/dxgkarg_close_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_create_device_record.hpp"
#include "miniport/ddi/dxgkarg_open_allocation_record.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/allocation_table.hpp"
#include "miniport/memory/shared_object_table.hpp"
#include "proto/resources/shared_allocation_data.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "miniport/driver/acquired_handle_data.hpp"
#include "miniport/driver/dxgkrnl_interface.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

DeviceTable::DeviceTable(Platform &platform, AllocationTable &allocations, SharedObjectTable &shared,
                         DxgkrnlInterface &dxgkrnl)
    : platform_(platform), allocations_(allocations), shared_(shared), dxgkrnl_(dxgkrnl),
      mutex_(platform, LockRank::device_table),
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
    const Status status = devices_.create(device, platform_, adapter, request.hDevice, request.Flags, request.hKmdProcess);
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
        if (open.shared() != nullptr) static_cast<void>(shared_.close(*open.shared()));
        static_cast<void>(allocations_.close(open.allocation()));
        return true;
    });
    return devices_.destroy(target);
}

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

Status DeviceTable::claim(const Allocation &allocation, const DxgkOpenAllocationInfoRecord &info,
                          SharedObject *&object)
{
    AV_SCOPE("DeviceTable::claim");
    object = nullptr;
    if (allocation.shared_id() == 0u) return Status::ok;
    SharedAllocationData data;
    if (data.decode(static_cast<const uint8_t *>(info.pPrivateDriverData), info.PrivateDriverDataSize) != Status::ok)
        return Status::invalid_parameter;
    const SharedObjectRecord &record = data.record();
    if (!record.assigned()) return adopt(allocation, info, data, object);
    if (record.object_id() != allocation.shared_id()) return Status::invalid_parameter;
    return shared_.open(record, object);
}

Status DeviceTable::adopt(const Allocation &allocation, const DxgkOpenAllocationInfoRecord &info,
                          SharedAllocationData &data, SharedObject *&object)
{
    AV_SCOPE("DeviceTable::adopt");
    SharedObjectRecord minted;
    const Status       opened = shared_.open_minted(allocation.shared_id(), object, minted);
    if (opened != Status::ok) return opened;
    static_cast<void>(data.mint(minted));
    __builtin_memcpy(info.pPrivateDriverData, data.bytes(), SharedAllocationData::encoded_size);
    return Status::ok;
}

Status DeviceTable::open_one(Device &device, DxgkOpenAllocationInfoRecord &info)
{
    AV_SCOPE("DeviceTable::open_one");
    Allocation *allocation = allocations_.find(info.hDeviceSpecificAllocation);
    info.hDeviceSpecificAllocation = nullptr;
    if (allocation == nullptr) return Status::invalid_parameter;
    SharedObject *object = nullptr;
    const Status  claimed = claim(*allocation, info, object);
    if (claimed != Status::ok) return claimed;
    ScopeExit give_back([&] { if (object != nullptr) static_cast<void>(shared_.close(*object)); });
    DeviceAllocation *open = nullptr;
    const Status created = opens_.create(open, device, *allocation, object);
    if (created != Status::ok) return created;
    const Status opened = allocations_.open(*allocation, &device, device.process());
    if (opened != Status::ok) {
        static_cast<void>(opens_.destroy(open));
        return opened;
    }
    give_back.dismiss();
    info.hDeviceSpecificAllocation = open;
    return Status::ok;
}

void DeviceTable::close_opened(DxgkargOpenAllocationRecord &request, uint32_t count)
{
    AV_SCOPE("DeviceTable::close_opened");
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        DxgkOpenAllocationInfoRecord &info = request.pOpenAllocation[index];
        auto *open = static_cast<DeviceAllocation *>(info.hDeviceSpecificAllocation);
        if (index < count) {
            if (open->shared() != nullptr) static_cast<void>(shared_.close(*open->shared()));
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
        if (open->shared() != nullptr) static_cast<void>(shared_.close(*open->shared()));
        static_cast<void>(allocations_.close(open->allocation()));
        static_cast<void>(opens_.destroy(open));
    }
    return Status::ok;
}

DeviceAllocation *DeviceTable::opened(void *handle)
{
    AV_SCOPE("DeviceTable::opened");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::opened");
    return opens_.find(handle);
}

Status DeviceTable::name_point(void *process, uint64_t surface, uint32_t point, NextPointRecord::Action action)
{
    AV_SCOPE("DeviceTable::name_point");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::name_point");
    if (process == nullptr) return Status::invalid_parameter;
    MutexGuard              lock(mutex_);
    DeviceAllocation *const open = opens_.find_if([process, surface](const DeviceAllocation &held) {
        return held.device().process() == process && held.allocation().shared_id() == surface;
    });
    if (open == nullptr) return Status::not_found;
    if (action == NextPointRecord::Action::name) open->name_point(point);
    else if (action == NextPointRecord::Action::pages) open->name_point(0u);
    else static_cast<void>(open->withdraw_point(point));
    return Status::ok;
}

Status DeviceTable::peek_point(void *handle, PointView &view)
{
    AV_SCOPE("DeviceTable::peek_point");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::peek_point");
    MutexGuard              lock(mutex_);
    DeviceAllocation *const open = handle != nullptr ? opens_.find(handle) : nullptr;
    if (open == nullptr) return Status::not_found;
    open->view(view);
    return Status::ok;
}

void DeviceTable::consume_point(void *handle)
{
    AV_SCOPE("DeviceTable::consume_point");
    platform_.require_irql_at_most(Irql::passive, "DeviceTable::consume_point");
    MutexGuard              lock(mutex_);
    DeviceAllocation *const open = handle != nullptr ? opens_.find(handle) : nullptr;
    if (open != nullptr) open->forget_point();
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

}
