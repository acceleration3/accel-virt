#include "device/ring/held_completions.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HeldCompletions::HeldCompletions() : fences_{}, marks_{}, head_(0u), count_(0u)
{
    AV_SCOPE("HeldCompletions::HeldCompletions");
}

bool HeldCompletions::hold(uint64_t fence, uint64_t mark)
{
    AV_SCOPE("HeldCompletions::hold");
    if (count_ == capacity) return false;
    const uint32_t slot = (head_ + count_) % capacity;
    fences_[slot] = fence;
    marks_[slot] = mark;
    count_++;
    return true;
}

uint64_t HeldCompletions::release(uint64_t reached)
{
    AV_SCOPE("HeldCompletions::release");
    uint64_t newest = 0u;
    while (count_ != 0u && marks_[head_] <= reached) {
        newest = fences_[head_];
        head_ = (head_ + 1u) % capacity;
        count_--;
    }
    return newest;
}

uint64_t HeldCompletions::release_all()
{
    AV_SCOPE("HeldCompletions::release_all");
    return release(UINT64_MAX);
}

bool HeldCompletions::empty() const
{
    AV_SCOPE("HeldCompletions::empty");
    return count_ == 0u;
}

bool HeldCompletions::invariants_hold() const
{
    AV_SCOPE("HeldCompletions::invariants_hold");
    if (count_ > capacity || head_ >= capacity) return false;
    for (uint32_t index = 1u; index < count_; index++)
        if (fences_[(head_ + index) % capacity] <= fences_[(head_ + index - 1u) % capacity]) return false;
    return true;
}

}  // namespace av
