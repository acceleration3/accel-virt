#include "guest/transport/transport_fence.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

TransportFence::TransportFence() : next_(1u), completed_(0u)
{
    AV_SCOPE("TransportFence::TransportFence");
}

uint64_t TransportFence::open()
{
    AV_SCOPE("TransportFence::open");
    if (outstanding() >= max_outstanding) return 0u;
    return next_++;
}

uint64_t TransportFence::peek() const
{
    AV_SCOPE("TransportFence::peek");
    return full() ? 0u : next_;
}

void TransportFence::close(uint64_t fence_id)
{
    AV_SCOPE("TransportFence::close");
    if (fence_id > completed_ && fence_id < next_) completed_ = fence_id;
}

bool TransportFence::pending(uint64_t fence_id) const
{
    AV_SCOPE("TransportFence::pending");
    return fence_id > completed_ && fence_id < next_;
}

uint64_t TransportFence::completed() const
{
    AV_SCOPE("TransportFence::completed");
    return completed_;
}

uint32_t TransportFence::outstanding() const
{
    AV_SCOPE("TransportFence::outstanding");
    return static_cast<uint32_t>(next_ - 1u - completed_);
}

uint64_t TransportFence::oldest() const
{
    AV_SCOPE("TransportFence::oldest");
    return outstanding() == 0u ? 0u : completed_ + 1u;
}

bool TransportFence::full() const
{
    AV_SCOPE("TransportFence::full");
    return outstanding() >= max_outstanding;
}

bool TransportFence::invariants_hold() const
{
    AV_SCOPE("TransportFence::invariants_hold");
    return next_ >= 1u && completed_ < next_ && outstanding() <= max_outstanding;
}

}  // namespace av
