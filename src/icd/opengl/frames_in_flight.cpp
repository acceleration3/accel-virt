#include "icd/opengl/frames_in_flight.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

FramesInFlight::FramesInFlight() : ends_{}, oldest_(0u)
{
    AV_SCOPE("FramesInFlight::FramesInFlight");
}

uint64_t FramesInFlight::end(uint64_t fence)
{
    AV_SCOPE("FramesInFlight::end");
    const uint64_t waited_for = ends_[oldest_];
    ends_[oldest_] = fence;
    oldest_ = (oldest_ + 1u) % depth;
    return waited_for;
}

void FramesInFlight::forget()
{
    AV_SCOPE("FramesInFlight::forget");
    for (uint64_t &fence : ends_) fence = 0u;
    oldest_ = 0u;
}

bool FramesInFlight::invariants_hold() const
{
    AV_SCOPE("FramesInFlight::invariants_hold");
    if (oldest_ >= depth) return false;
    for (uint32_t step = 1u; step < depth; step++)
        if (ends_[(oldest_ + step) % depth] < ends_[(oldest_ + step - 1u) % depth]) return false;
    return true;
}

}  // namespace av::glicd
