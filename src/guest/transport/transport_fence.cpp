#include "guest/transport/transport_fence.hpp"

namespace av {

TransportFence::TransportFence() : next_(1u), completed_(0u) {}

uint64_t TransportFence::open()
{
    if (outstanding() >= max_outstanding) return 0u;
    return next_++;
}

uint64_t TransportFence::peek() const { return full() ? 0u : next_; }

void TransportFence::close(uint64_t fence_id)
{
    if (fence_id > completed_ && fence_id < next_) completed_ = fence_id;
}

bool TransportFence::pending(uint64_t fence_id) const
{
    return fence_id > completed_ && fence_id < next_;
}

uint64_t TransportFence::completed() const { return completed_; }

uint32_t TransportFence::outstanding() const
{
    return static_cast<uint32_t>(next_ - 1u - completed_);
}

uint64_t TransportFence::oldest() const { return outstanding() == 0u ? 0u : completed_ + 1u; }

bool TransportFence::full() const { return outstanding() >= max_outstanding; }

bool TransportFence::invariants_hold() const
{
    return next_ >= 1u && completed_ < next_ && outstanding() <= max_outstanding;
}

}  // namespace av
