#include "device/execution/context_shared_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/context_executor.hpp"
#include "device/memory/host_shared_objects.hpp"

namespace av {

ContextSharedObjects::ContextSharedObjects(HostSharedObjects &objects, ContextExecutor &executor)
    : objects_(objects), executor_(executor), slots_{}
{
    AV_SCOPE("ContextSharedObjects::ContextSharedObjects");
}

ContextSharedObjects::~ContextSharedObjects()
{
    AV_SCOPE("ContextSharedObjects::~ContextSharedObjects");
    release_all();
}

void ContextSharedObjects::release_all()
{
    AV_SCOPE("ContextSharedObjects::release_all");
    /* The executor is gone by now and took every object it made with it, so
     * there is nothing here to destroy - only references to let go of. */
    for (ContextSharedObjectSlot &slot : slots_) {
        if (slot.object_id_ == 0u) continue;
        objects_.close(slot.object_id_);
        slot = ContextSharedObjectSlot{};
    }
}

ContextSharedObjectSlot *ContextSharedObjects::slot_of(uint64_t object_id)
{
    AV_SCOPE("ContextSharedObjects::slot_of");
    for (ContextSharedObjectSlot &slot : slots_)
        if (slot.object_id_ == object_id) return &slot;
    return nullptr;
}

ContextSharedObjectSlot *ContextSharedObjects::free_slot()
{
    AV_SCOPE("ContextSharedObjects::free_slot");
    for (ContextSharedObjectSlot &slot : slots_)
        if (slot.object_id_ == 0u) return &slot;
    return nullptr;
}

void ContextSharedObjects::give_back(ContextSharedObjectSlot &slot)
{
    AV_SCOPE("ContextSharedObjects::give_back");
    /* A publisher made nothing of its own: it named an object its guest
     * already held, so there is nothing of this context's to destroy - and the
     * hold its publish made is the guest's, which goes only when the driver
     * says every process let go of the object (SharedReleaseRequest), not
     * when this context does. An opener's hold is its own. */
    if (!slot.published_) {
        executor_.release_object(slot.kind_, slot.wire_handle_);
        objects_.close(slot.object_id_);
    }
    slot = ContextSharedObjectSlot{};
}

Status ContextSharedObjects::publish(uint64_t object_id, uint32_t kind, uint64_t wire_handle,
                                     uint64_t size)
{
    AV_SCOPE("ContextSharedObjects::publish");
    ContextSharedObjectSlot *const held = slot_of(object_id);
    if (held != nullptr) {
        /* The same context publishing the same thing again is a guest that
         * shared one object twice, which Windows lets it do. */
        const bool same = held->published_ && held->kind_ == kind && held->wire_handle_ == wire_handle;
        return same ? Status::ok : Status::conflict;
    }
    ContextSharedObjectSlot *const slot = free_slot();
    if (slot == nullptr) return Status::no_space;

    uint32_t     handle_kind = 0u;
    int          descriptor = -1;
    const Status exported = executor_.export_object(kind, wire_handle, handle_kind, descriptor);
    if (exported != Status::ok) return exported;

    const Status published = objects_.publish(object_id, kind, handle_kind, size, descriptor);
    if (published != Status::ok) return published;
    *slot = ContextSharedObjectSlot{object_id, wire_handle, kind, true};
    return Status::ok;
}

Status ContextSharedObjects::open(uint64_t object_id, uint32_t kind, uint64_t size,
                                  uint64_t &wire_handle)
{
    AV_SCOPE("ContextSharedObjects::open");
    wire_handle = 0u;
    if (slot_of(object_id) != nullptr) return Status::conflict;
    ContextSharedObjectSlot *const slot = free_slot();
    if (slot == nullptr) return Status::no_space;

    uint32_t     handle_kind = 0u;
    uint64_t     held_size = 0u;
    int          descriptor = -1;
    const Status opened = objects_.open(object_id, kind, size, handle_kind, held_size, descriptor);
    if (opened != Status::ok) return opened;

    uint64_t     made = 0u;
    const Status imported = executor_.import_object(kind, handle_kind, descriptor, held_size, made);
    if (imported != Status::ok) {
        objects_.close(object_id);
        return imported;
    }
    *slot = ContextSharedObjectSlot{object_id, made, kind, false};
    wire_handle = made;
    return Status::ok;
}

void ContextSharedObjects::release(uint64_t object_id)
{
    AV_SCOPE("ContextSharedObjects::release");
    if (object_id == 0u) return;
    ContextSharedObjectSlot *const slot = slot_of(object_id);
    if (slot != nullptr) give_back(*slot);
}

uint32_t ContextSharedObjects::held() const
{
    AV_SCOPE("ContextSharedObjects::held");
    uint32_t count = 0u;
    for (const ContextSharedObjectSlot &slot : slots_)
        if (slot.object_id_ != 0u) count++;
    return count;
}

bool ContextSharedObjects::invariants_hold() const
{
    AV_SCOPE("ContextSharedObjects::invariants_hold");
    for (uint32_t index = 0u; index < max_objects; index++) {
        const ContextSharedObjectSlot &slot = slots_[index];
        if (slot.object_id_ == 0u) {
            if (slot.wire_handle_ != 0u || slot.kind_ != 0u || slot.published_) return false;
            continue;
        }
        if (slot.wire_handle_ == 0u || slot.kind_ == 0u) return false;
        for (uint32_t other = index + 1u; other < max_objects; other++)
            if (slots_[other].object_id_ == slot.object_id_) return false;
    }
    return true;
}

}  // namespace av
