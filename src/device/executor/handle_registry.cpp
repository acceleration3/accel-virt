#include "device/executor/handle_registry.hpp"

namespace av {

HandleRegistry::HandleRegistry() : handles_{}, count_(0u) {}

bool HandleRegistry::remember(uint64_t handle)
{
    /* The null handle is the API's own and is never anybody's to own. */
    if (handle == 0u) return true;
    if (knows(handle)) return true;
    if (count_ >= max_handles) return false;
    handles_[count_++] = handle;
    return true;
}

bool HandleRegistry::knows(uint64_t handle) const
{
    if (handle == 0u) return true;
    for (uint32_t i = 0u; i < count_; i++) {
        if (handles_[i] == handle) return true;
    }
    return false;
}

void HandleRegistry::forget(uint64_t handle)
{
    if (handle == 0u) return;
    for (uint32_t i = 0u; i < count_; i++) {
        if (handles_[i] != handle) continue;
        handles_[i] = handles_[count_ - 1u];
        count_--;
        return;
    }
}

void HandleRegistry::forget_all() { count_ = 0u; }

uint32_t HandleRegistry::count() const { return count_; }

bool HandleRegistry::invariants_hold() const
{
    if (count_ > max_handles) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (handles_[i] == 0u) return false;
    }
    return true;
}

}  // namespace av
