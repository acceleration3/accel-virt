#include "device/executor/handle_registry.hpp"

namespace av {

HandleRegistry::HandleRegistry() : handles_{}, parents_{}, end_(0u), count_(0u)
{
    static_assert(max_handles < no_parent);
    for (uint16_t &parent : parents_) parent = no_parent;
}

bool HandleRegistry::remember(uint64_t handle, uint64_t parent)
{
    /* The null handle is the API's own and is never anybody's to own. */
    if (handle == 0u) return true;
    if (knows(handle)) return true;
    uint32_t slot = 0u;
    while (slot < end_ && handles_[slot] != 0u) slot++;
    if (slot == max_handles) return false;
    const uint32_t parent_slot = parent != 0u ? slot_of(parent) : end_;
    handles_[slot] = handle;
    parents_[slot] = parent_slot != end_ ? static_cast<uint16_t>(parent_slot) : no_parent;
    if (slot == end_) end_++;
    count_++;
    return true;
}

bool HandleRegistry::knows(uint64_t handle) const
{
    if (handle == 0u) return true;
    return slot_of(handle) != end_;
}

void HandleRegistry::forget(uint64_t handle)
{
    if (handle == 0u) return;
    const uint32_t slot = slot_of(handle);
    if (slot == end_) return;
    free_slot(slot);
    sweep_orphans();
}

void HandleRegistry::forget_children(uint64_t handle)
{
    if (handle == 0u) return;
    const uint32_t slot = slot_of(handle);
    if (slot == end_) return;
    /* A free slot's parent is no_parent, which no slot is, so this frees only
     * live children. */
    for (uint32_t i = 0u; i < end_; i++) {
        if (parents_[i] == slot) free_slot(i);
    }
    sweep_orphans();
}

void HandleRegistry::forget_all()
{
    for (uint32_t i = 0u; i < end_; i++) {
        handles_[i] = 0u;
        parents_[i] = no_parent;
    }
    end_ = 0u;
    count_ = 0u;
}

uint32_t HandleRegistry::count() const { return count_; }

uint32_t HandleRegistry::slot_of(uint64_t handle) const
{
    for (uint32_t i = 0u; i < end_; i++) {
        if (handles_[i] == handle) return i;
    }
    return end_;
}

void HandleRegistry::free_slot(uint32_t slot)
{
    handles_[slot] = 0u;
    parents_[slot] = no_parent;
    count_--;
}

void HandleRegistry::sweep_orphans()
{
    bool freed = true;
    while (freed) {
        freed = false;
        for (uint32_t i = 0u; i < end_; i++) {
            if (handles_[i] == 0u || parents_[i] == no_parent) continue;
            if (handles_[parents_[i]] != 0u) continue;
            free_slot(i);
            freed = true;
        }
    }
    while (end_ != 0u && handles_[end_ - 1u] == 0u) end_--;
}

bool HandleRegistry::invariants_hold() const
{
    if (end_ > max_handles) return false;
    if (end_ != 0u && handles_[end_ - 1u] == 0u) return false;
    uint32_t held = 0u;
    for (uint32_t i = 0u; i < end_; i++) {
        const uint16_t parent = parents_[i];
        if (handles_[i] == 0u) {
            if (parent != no_parent) return false;
            continue;
        }
        held++;
        if (parent == no_parent) continue;
        if (parent >= end_ || parent == i || handles_[parent] == 0u) return false;
    }
    return held == count_;
}

}  // namespace av
