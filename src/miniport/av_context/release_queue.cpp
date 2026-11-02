#include "miniport/av_context/release_queue.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ReleaseQueue::ReleaseQueue() : held_{}, count_(0u), next_(0u), active_(false)
{
    AV_SCOPE("ReleaseQueue::ReleaseQueue");
}

ReleaseQueue::Turn ReleaseQueue::arrive(const SubmissionDescriptor &batch, uint64_t tail)
{
    AV_SCOPE("ReleaseQueue::arrive");
    if (!active_) {
        active_ = true;
        next_ = tail;
    }
    if (batch.end() <= next_) return Turn::released_already;
    if (batch.begin() < next_) return Turn::refused;
    if (batch.begin() == next_) return Turn::release_now;
    for (uint32_t index = 0u; index < count_; index++)
        if (held_[index].begin() == batch.begin()) return Turn::held;
    if (count_ == most_held) return Turn::refused;
    held_[count_++] = batch;
    return Turn::held;
}

void ReleaseQueue::released(const SubmissionDescriptor &batch)
{
    AV_SCOPE("ReleaseQueue::released");
    next_ = batch.end();
}

bool ReleaseQueue::next_held(SubmissionDescriptor &batch)
{
    AV_SCOPE("ReleaseQueue::next_held");
    for (uint32_t index = 0u; index < count_; index++) {
        if (held_[index].begin() != next_) continue;
        batch = held_[index];
        held_[index] = held_[--count_];
        return true;
    }
    return false;
}

bool ReleaseQueue::active() const
{
    AV_SCOPE("ReleaseQueue::active");
    return active_;
}

void ReleaseQueue::clear()
{
    AV_SCOPE("ReleaseQueue::clear");
    count_ = 0u;
    next_ = 0u;
    active_ = false;
}

uint32_t ReleaseQueue::held() const
{
    AV_SCOPE("ReleaseQueue::held");
    return count_;
}

}
