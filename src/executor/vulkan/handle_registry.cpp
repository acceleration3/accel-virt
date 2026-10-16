#include "executor/vulkan/handle_registry.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HandleRegistry::HandleRegistry(BlockSource &blocks) : slots_(blocks, Pool::paged)
{
    AV_SCOPE("HandleRegistry::HandleRegistry");
}

Status HandleRegistry::make_room(uint64_t handles)
{
    AV_SCOPE("HandleRegistry::make_room");
    const uint64_t wanted = uint64_t{slots_.count()} + handles;
    if (wanted > KeyedSlots<uint64_t, HandleSlot>::most_entries) return Status::no_memory;
    return slots_.reserve(static_cast<uint32_t>(wanted));
}

Status HandleRegistry::remember(uint64_t handle, uint64_t parent, HandleKind kind)
{
    AV_SCOPE("HandleRegistry::remember");
    /* The null handle is the API's own and is never anybody's to own. */
    if (handle == 0u || knows(handle)) return Status::ok;
    const uint32_t parent_slot = parent != 0u ? slots_.slot_of(parent) : no_slot;
    uint32_t       slot = no_slot;
    const Status   inserted = slots_.insert(handle, HandleSlot{parent_slot, 0u, kind}, slot);
    if (inserted != Status::ok) return inserted;
    if (parent_slot != no_slot) slots_.at(parent_slot)->children_++;
    return Status::ok;
}

bool HandleRegistry::knows(uint64_t handle) const
{
    AV_SCOPE("HandleRegistry::knows");
    return handle == 0u || slots_.find(handle) != nullptr;
}

bool HandleRegistry::knows(uint64_t handle, HandleKind kind) const
{
    AV_SCOPE("HandleRegistry::knows");
    const HandleSlot *const slot = handle != 0u ? slots_.find(handle) : nullptr;
    return slot != nullptr && slot->kind_ == kind;
}

void HandleRegistry::forget(uint64_t handle)
{
    AV_SCOPE("HandleRegistry::forget");
    const uint32_t slot = handle != 0u ? slots_.slot_of(handle) : no_slot;
    if (slot == no_slot) return;
    const bool had_children = slots_.at(slot)->children_ != 0u;
    free_slot(slot);
    if (had_children) sweep_orphans();
}

void HandleRegistry::forget_children(uint64_t handle)
{
    AV_SCOPE("HandleRegistry::forget_children");
    const uint32_t slot = handle != 0u ? slots_.slot_of(handle) : no_slot;
    if (slot == no_slot || slots_.at(slot)->children_ == 0u) return;
    bool grandchildren = false;
    for (uint32_t child = slots_.next_live(0u); child != no_slot && slots_.at(slot)->children_ != 0u;
         child = slots_.next_live(child + 1u)) {
        const HandleSlot &made = *slots_.at(child);
        if (made.parent_ != slot) continue;
        grandchildren = grandchildren || made.children_ != 0u;
        free_slot(child);
    }
    if (grandchildren) sweep_orphans();
}

void HandleRegistry::forget_all()
{
    AV_SCOPE("HandleRegistry::forget_all");
    slots_.clear();
}

uint32_t HandleRegistry::count() const
{
    AV_SCOPE("HandleRegistry::count");
    return slots_.count();
}

void HandleRegistry::free_slot(uint32_t slot)
{
    AV_SCOPE("HandleRegistry::free_slot");
    const uint32_t parent = slots_.at(slot)->parent_;
    if (parent != no_slot && slots_.at(parent) != nullptr) slots_.at(parent)->children_--;
    slots_.erase_slot(slot);
}

void HandleRegistry::sweep_orphans()
{
    AV_SCOPE("HandleRegistry::sweep_orphans");
    bool freed = true;
    while (freed) {
        freed = false;
        for (uint32_t slot = slots_.next_live(0u); slot != no_slot; slot = slots_.next_live(slot + 1u)) {
            const uint32_t parent = slots_.at(slot)->parent_;
            if (parent == no_slot || slots_.at(parent) != nullptr) continue;
            free_slot(slot);
            freed = true;
        }
    }
}

bool HandleRegistry::invariants_hold() const
{
    AV_SCOPE("HandleRegistry::invariants_hold");
    uint64_t made_through = 0u;
    uint64_t claimed = 0u;
    for (uint32_t slot = slots_.next_live(0u); slot != no_slot; slot = slots_.next_live(slot + 1u)) {
        const HandleSlot &held = *slots_.at(slot);
        if (slots_.key_at(slot) == 0u) return false;
        claimed += held.children_;
        if (held.parent_ == no_slot) continue;
        if (held.parent_ == slot || slots_.at(held.parent_) == nullptr) return false;
        made_through++;
    }
    return claimed == made_through;
}

}  // namespace av
