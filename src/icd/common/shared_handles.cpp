#include "icd/common/shared_handles.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/thread_lock_hold.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_record.hpp"

namespace av::icd {

SharedHandles::SharedHandles(Kmt &kmt, ThreadLock &lock, BlockSource &memory)
    : kmt_(kmt), lock_(lock), slots_(memory)
{
    AV_SCOPE("SharedHandles::SharedHandles");
}

SharedHandleSlot *SharedHandles::row_of(uint64_t handle)
{
    AV_SCOPE("SharedHandles::row_of");
    if (handle == 0u) return nullptr;
    return slots_.find_if([handle](const SharedHandleSlot &row) { return row.handle_ == handle; });
}

const SharedHandleSlot *SharedHandles::row_of(uint64_t handle) const
{
    AV_SCOPE("SharedHandles::row_of");
    if (handle == 0u) return nullptr;
    return slots_.find_if([handle](const SharedHandleSlot &row) { return row.handle_ == handle; });
}

SharedHandleSlot *SharedHandles::row_for(uint64_t object_id)
{
    AV_SCOPE("SharedHandles::row_for");
    return slots_.find_if([object_id](const SharedHandleSlot &row) { return row.object_id_ == object_id; });
}

/* The row is taken before the kernel is asked, so a handle the kernel made is
 * never one this table has nowhere to keep. */
Status SharedHandles::mint(uint32_t device, uint32_t carrier, uint64_t object_id, uint64_t &handle)
{
    AV_SCOPE("SharedHandles::mint");
    handle = 0u;
    if (object_id == 0u) return Status::invalid_parameter;
    ThreadLockHold held(lock_);
    const SharedHandleSlot *const already = row_for(object_id);
    if (already != nullptr) {
        handle = already->handle_;
        return Status::ok;
    }
    SharedHandleSlot *row = nullptr;
    const Status      room = slots_.create(row, SharedHandleSlot{0u, object_id, device, 0u});
    if (room != Status::ok) return room;
    uint64_t     made = 0u;
    const Status shared = kmt_.share_object(device, carrier, made);
    if (shared != Status::ok) {
        static_cast<void>(slots_.destroy(row));
        return shared;
    }
    row->handle_ = made;
    handle = made;
    return Status::ok;
}

Status SharedHandles::adopt(uint32_t device, uint64_t handle, uint64_t &object_id)
{
    AV_SCOPE("SharedHandles::adopt");
    object_id = 0u;
    if (handle == 0u) return Status::invalid_parameter;
    ThreadLockHold held(lock_);
    const SharedHandleSlot *const already = row_of(handle);
    if (already != nullptr) {
        object_id = already->object_id_;
        return Status::ok;
    }
    SharedHandleSlot *row = nullptr;
    const Status      room = slots_.create(row, SharedHandleSlot{0u, 0u, device, 0u});
    if (room != Status::ok) return room;
    uint8_t      carried[max_private] = {};
    uint32_t     written = 0u;
    uint32_t     resource = 0u;
    const Status opened = kmt_.open_shared_object(device, handle, resource, carried, max_private, written);
    if (opened != Status::ok) {
        static_cast<void>(slots_.destroy(row));
        return opened;
    }
    /* What came back is the creating process's bytes, which this process did
     * not write: a record that is not there, or one with no id minted into it,
     * names no object and the open is given back. */
    SharedObjectRecord record;
    const bool         carries =
        written == AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size &&
        record.decode(carried + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size) ==
            Status::ok &&
        record.assigned();
    if (!carries) {
        static_cast<void>(kmt_.destroy_shared_allocation(device, resource));
        static_cast<void>(slots_.destroy(row));
        return Status::invalid_parameter;
    }
    *row = SharedHandleSlot{handle, record.object_id(), device, resource};
    object_id = record.object_id();
    return Status::ok;
}

bool SharedHandles::resolve(uint64_t handle, uint64_t &object_id) const
{
    AV_SCOPE("SharedHandles::resolve");
    ThreadLockHold held(lock_);
    const SharedHandleSlot *const row = row_of(handle);
    if (row == nullptr) return false;
    object_id = row->object_id_;
    return true;
}

void SharedHandles::give_back(const SharedHandleSlot &row)
{
    AV_SCOPE("SharedHandles::give_back");
    static_cast<void>(kmt_.close_shared_object(row.handle_));
    /* Only what an adopt opened: a minted handle's carrier is the
     * application's, and destroying it would take away what it is using. */
    if (row.resource_ != 0u)
        static_cast<void>(kmt_.destroy_shared_allocation(row.device_, row.resource_));
}

void SharedHandles::forget(uint64_t handle)
{
    AV_SCOPE("SharedHandles::forget");
    ThreadLockHold          held(lock_);
    SharedHandleSlot *const row = row_of(handle);
    if (row == nullptr) return;
    give_back(*row);
    static_cast<void>(slots_.destroy(row));
}

void SharedHandles::forget_device(uint32_t device)
{
    AV_SCOPE("SharedHandles::forget_device");
    ThreadLockHold held(lock_);
    slots_.destroy_if([this, device](const SharedHandleSlot &row) {
        if (row.device_ != device) return false;
        give_back(row);
        return true;
    });
}

uint32_t SharedHandles::live() const
{
    AV_SCOPE("SharedHandles::live");
    ThreadLockHold held(lock_);
    return slots_.live();
}

/* Every row names a handle and an object, and no two name either alike. */
bool SharedHandles::invariants_hold() const
{
    AV_SCOPE("SharedHandles::invariants_hold");
    return slots_.all_of([this](const SharedHandleSlot &row) {
        if (row.handle_ == 0u || row.object_id_ == 0u) return false;
        uint32_t alike = 0u;
        slots_.for_each([&row, &alike](const SharedHandleSlot &other) {
            if (other.handle_ == row.handle_ || other.object_id_ == row.object_id_) alike++;
        });
        return alike == 1u;
    });
}

}  // namespace av::icd
