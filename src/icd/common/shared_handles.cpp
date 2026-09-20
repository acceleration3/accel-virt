#include "icd/common/shared_handles.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/thread_lock.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_record.hpp"

namespace av::icd {

namespace {

/* Holds `lock` for the length of a call, as the rest of the ICD does. */
class Held {
public:
    explicit Held(ThreadLock &lock) : lock_(lock)
    {
        AV_SCOPE("Held::Held");
        lock_.acquire();
    }
    ~Held()
    {
        AV_SCOPE("Held::~Held");
        lock_.release();
    }
    Held(const Held &) = delete;
    Held &operator=(const Held &) = delete;

private:
    ThreadLock &lock_;
};

}  // namespace

SharedHandles::SharedHandles(Kmt &kmt, ThreadLock &lock) : kmt_(kmt), lock_(lock), slots_(), live_(0u)
{
    AV_SCOPE("SharedHandles::SharedHandles");
}

SharedHandleSlot *SharedHandles::row_of(uint64_t handle)
{
    AV_SCOPE("SharedHandles::row_of");
    if (handle == 0u) return nullptr;
    for (uint32_t index = 0u; index < live_; index++)
        if (slots_[index].handle_ == handle) return &slots_[index];
    return nullptr;
}

const SharedHandleSlot *SharedHandles::row_of(uint64_t handle) const
{
    AV_SCOPE("SharedHandles::row_of");
    if (handle == 0u) return nullptr;
    for (uint32_t index = 0u; index < live_; index++)
        if (slots_[index].handle_ == handle) return &slots_[index];
    return nullptr;
}

SharedHandleSlot *SharedHandles::row_for(uint64_t object_id)
{
    AV_SCOPE("SharedHandles::row_for");
    for (uint32_t index = 0u; index < live_; index++)
        if (slots_[index].object_id_ == object_id) return &slots_[index];
    return nullptr;
}

Status SharedHandles::mint(uint32_t device, uint32_t carrier, uint64_t object_id, uint64_t &handle)
{
    AV_SCOPE("SharedHandles::mint");
    handle = 0u;
    if (object_id == 0u) return Status::invalid_parameter;
    Held held(lock_);
    const SharedHandleSlot *const already = row_for(object_id);
    if (already != nullptr) {
        handle = already->handle_;
        return Status::ok;
    }
    if (live_ == max_handles) return Status::no_space;
    uint64_t     made = 0u;
    const Status shared = kmt_.share_object(device, carrier, made);
    if (shared != Status::ok) return shared;
    slots_[live_] = SharedHandleSlot{made, object_id, device, 0u};
    live_++;
    handle = made;
    return Status::ok;
}

Status SharedHandles::adopt(uint32_t device, uint64_t handle, uint64_t &object_id)
{
    AV_SCOPE("SharedHandles::adopt");
    object_id = 0u;
    if (handle == 0u) return Status::invalid_parameter;
    Held held(lock_);
    const SharedHandleSlot *const already = row_of(handle);
    if (already != nullptr) {
        object_id = already->object_id_;
        return Status::ok;
    }
    if (live_ == max_handles) return Status::no_space;
    uint8_t      carried[max_private] = {};
    uint32_t     written = 0u;
    uint32_t     resource = 0u;
    const Status opened = kmt_.open_shared_object(device, handle, resource, carried, max_private, written);
    if (opened != Status::ok) return opened;
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
        return Status::invalid_parameter;
    }
    slots_[live_] = SharedHandleSlot{handle, record.object_id(), device, resource};
    live_++;
    object_id = record.object_id();
    return Status::ok;
}

bool SharedHandles::resolve(uint64_t handle, uint64_t &object_id) const
{
    AV_SCOPE("SharedHandles::resolve");
    Held held(lock_);
    const SharedHandleSlot *const row = row_of(handle);
    if (row == nullptr) return false;
    object_id = row->object_id_;
    return true;
}

void SharedHandles::give_back(SharedHandleSlot &row)
{
    AV_SCOPE("SharedHandles::give_back");
    static_cast<void>(kmt_.close_shared_object(row.handle_));
    /* Only what an adopt opened: a minted handle's carrier is the
     * application's, and destroying it would take away what it is using. */
    if (row.resource_ != 0u)
        static_cast<void>(kmt_.destroy_shared_allocation(row.device_, row.resource_));
    live_--;
    row = slots_[live_];
}

void SharedHandles::forget(uint64_t handle)
{
    AV_SCOPE("SharedHandles::forget");
    Held              held(lock_);
    SharedHandleSlot *const row = row_of(handle);
    if (row == nullptr) return;
    give_back(*row);
}

void SharedHandles::forget_device(uint32_t device)
{
    AV_SCOPE("SharedHandles::forget_device");
    Held     held(lock_);
    uint32_t index = 0u;
    while (index < live_) {
        if (slots_[index].device_ != device) {
            index++;
            continue;
        }
        /* give_back moves the last row into this one, so the same index is
         * looked at again. */
        give_back(slots_[index]);
    }
}

uint32_t SharedHandles::live() const
{
    AV_SCOPE("SharedHandles::live");
    Held held(lock_);
    return live_;
}

bool SharedHandles::invariants_hold() const
{
    AV_SCOPE("SharedHandles::invariants_hold");
    if (live_ > max_handles) return false;
    for (uint32_t index = 0u; index < live_; index++) {
        if (slots_[index].handle_ == 0u || slots_[index].object_id_ == 0u) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (slots_[earlier].handle_ == slots_[index].handle_ ||
                slots_[earlier].object_id_ == slots_[index].object_id_)
                return false;
    }
    return true;
}

}  // namespace av::icd
