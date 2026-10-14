#include "icd/opengl/frames_in_flight.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

FramesInFlight::FramesInFlight() : fences_{}, ended_at_{}, oldest_(0u), count_(0u)
{
    AV_SCOPE("FramesInFlight::FramesInFlight");
}

uint64_t FramesInFlight::end(uint64_t fence, uint64_t now, uint64_t frequency, uint64_t completed)
{
    AV_SCOPE("FramesInFlight::end");
    while (count_ != 0u && fences_[oldest_] <= completed) {
        oldest_ = (oldest_ + 1u) % most;
        count_--;
    }
    if (fence <= completed) return 0u;
    uint64_t waited_for = 0u;
    if (count_ == most) {
        waited_for = fences_[oldest_];
        oldest_ = (oldest_ + 1u) % most;
        count_--;
    }
    const uint32_t slot = (oldest_ + count_) % most;
    fences_[slot] = fence;
    ended_at_[slot] = now;
    count_++;
    const uint64_t latency = frequency / 1000u * latency_ms;
    for (uint32_t step = 0u; step < count_; step++) {
        const uint32_t at = (oldest_ + step) % most;
        if (now - ended_at_[at] < latency) break;
        waited_for = fences_[at];
    }
    return waited_for;
}

void FramesInFlight::forget()
{
    AV_SCOPE("FramesInFlight::forget");
    oldest_ = 0u;
    count_ = 0u;
}

uint32_t FramesInFlight::unfinished() const
{
    AV_SCOPE("FramesInFlight::unfinished");
    return count_;
}

bool FramesInFlight::invariants_hold() const
{
    AV_SCOPE("FramesInFlight::invariants_hold");
    if (oldest_ >= most || count_ > most) return false;
    for (uint32_t step = 1u; step < count_; step++) {
        const uint32_t at = (oldest_ + step) % most;
        const uint32_t before = (oldest_ + step - 1u) % most;
        if (fences_[at] < fences_[before] || ended_at_[at] < ended_at_[before]) return false;
    }
    return true;
}

}  // namespace av::glicd
