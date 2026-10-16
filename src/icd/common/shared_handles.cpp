#include "icd/common/shared_handles.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "proto/resources/shared_allocation_data.hpp"

namespace av::icd {

SharedHandles::SharedHandles(Kmt &kmt, ThreadLock &lock, BlockSource &memory)
    : kmt_(kmt), lock_(lock), slots_(memory, Pool::paged), handles_of_(memory, Pool::paged)
{
    AV_SCOPE("SharedHandles::SharedHandles");
}

Status SharedHandles::make_room()
{
    AV_SCOPE("SharedHandles::make_room");
    const Status rows = slots_.reserve(slots_.count() + 1u);
    if (rows != Status::ok) return rows;
    return handles_of_.reserve(handles_of_.count() + 1u);
}

void SharedHandles::keep(const SharedHandleSlot &row)
{
    AV_SCOPE("SharedHandles::keep");
    uint32_t slot = 0u;
    static_cast<void>(slots_.insert(row.handle_, row, slot));
    /* An object already held under another handle keeps that one. */
    static_cast<void>(handles_of_.insert(row.object_id_, row.handle_, slot));
}

/* Room is made before the kernel is asked, so a handle the kernel made is
 * never one this table has nowhere to keep. */
Status SharedHandles::mint(uint32_t device, uint32_t carrier, uint64_t object_id, uint64_t &handle)
{
    AV_SCOPE("SharedHandles::mint");
    handle = 0u;
    if (object_id == 0u) return Status::invalid_parameter;
    ThreadLockHold held(lock_);
    const uint64_t *const already = handles_of_.find(object_id);
    if (already != nullptr) {
        handle = *already;
        return Status::ok;
    }
    const Status room = make_room();
    if (room != Status::ok) return room;
    uint64_t     made = 0u;
    const Status shared = kmt_.share_object(device, carrier, made);
    if (shared != Status::ok) return shared;
    keep(SharedHandleSlot{made, object_id, device, 0u});
    handle = made;
    return Status::ok;
}

Status SharedHandles::adopt(uint32_t device, uint64_t handle, uint64_t &object_id)
{
    AV_SCOPE("SharedHandles::adopt");
    object_id = 0u;
    if (handle == 0u) return Status::invalid_parameter;
    ThreadLockHold held(lock_);
    const SharedHandleSlot *const already = slots_.find(handle);
    if (already != nullptr) {
        object_id = already->object_id_;
        return Status::ok;
    }
    const Status room = make_room();
    if (room != Status::ok) return room;
    uint8_t      carried[max_private] = {};
    uint32_t     written = 0u;
    uint32_t     resource = 0u;
    const Status opened = kmt_.open_shared_object(device, handle, resource, carried, max_private, written);
    if (opened != Status::ok) return opened;
    /* What came back is the creating process's bytes, which this process did
     * not write: a record that is not there, or one with no id minted into it,
     * names no object and the open is given back. */
    SharedAllocationData data;
    if (data.decode(carried, written) != Status::ok || !data.assigned()) {
        static_cast<void>(kmt_.destroy_shared_allocation(device, resource));
        return Status::invalid_parameter;
    }
    keep(SharedHandleSlot{handle, data.object_id(), device, resource});
    object_id = data.object_id();
    return Status::ok;
}

bool SharedHandles::resolve(uint64_t handle, uint64_t &object_id) const
{
    AV_SCOPE("SharedHandles::resolve");
    ThreadLockHold                held(lock_);
    const SharedHandleSlot *const row = handle != 0u ? slots_.find(handle) : nullptr;
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
    const uint64_t *const named = handles_of_.find(row.object_id_);
    if (named != nullptr && *named == row.handle_) static_cast<void>(handles_of_.erase(row.object_id_));
}

void SharedHandles::forget(uint64_t handle)
{
    AV_SCOPE("SharedHandles::forget");
    ThreadLockHold   held(lock_);
    SharedHandleSlot row{};
    if (handle == 0u || !slots_.take(handle, row)) return;
    give_back(row);
}

void SharedHandles::forget_device(uint32_t device)
{
    AV_SCOPE("SharedHandles::forget_device");
    ThreadLockHold held(lock_);
    slots_.erase_if([this, device](uint64_t, SharedHandleSlot &row) {
        if (row.device_ != device) return false;
        give_back(row);
        return true;
    });
}

uint32_t SharedHandles::live() const
{
    AV_SCOPE("SharedHandles::live");
    ThreadLockHold held(lock_);
    return slots_.count();
}

/* Every row names a handle and an object under its own handle, and every
 * object's handle is a row of that object. */
bool SharedHandles::invariants_hold() const
{
    AV_SCOPE("SharedHandles::invariants_hold");
    bool whole = slots_.invariants_hold() && handles_of_.invariants_hold();
    slots_.for_each([&whole](uint64_t handle, const SharedHandleSlot &row) {
        whole = whole && row.handle_ == handle && handle != 0u && row.object_id_ != 0u;
    });
    handles_of_.for_each([this, &whole](uint64_t object_id, const uint64_t &handle) {
        const SharedHandleSlot *const row = slots_.find(handle);
        whole = whole && row != nullptr && row->object_id_ == object_id;
    });
    return whole;
}

}  // namespace av::icd
