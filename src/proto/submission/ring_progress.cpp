#include "proto/submission/ring_progress.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

RingProgress::RingProgress() : started_(false), order_(0u), tail_(0u), head_(0u)
{
    AV_SCOPE("RingProgress::RingProgress");
}

bool RingProgress::consistent(uint8_t order, uint64_t tail, uint64_t head) const
{
    AV_SCOPE("RingProgress::consistent");
    if (order < RingHeader::lowest_order || order > RingHeader::highest_order) return false;
    return head <= tail && tail - head <= (uint64_t{1} << order);
}

Status RingProgress::observe(const RingHeader &snapshot)
{
    AV_SCOPE("RingProgress::observe");
    if (!consistent(snapshot.order(), snapshot.tail(), snapshot.head())) return Status::invalid_state;
    if (started_ && (snapshot.order() != order_ || snapshot.head() < head_ || snapshot.tail() < tail_))
        return Status::invalid_state;
    started_ = true;
    order_ = snapshot.order();
    tail_ = snapshot.tail();
    head_ = snapshot.head();
    return Status::ok;
}

bool     RingProgress::started() const
{
    AV_SCOPE("RingProgress::started");
    return started_;
}
uint8_t  RingProgress::order() const
{
    AV_SCOPE("RingProgress::order");
    return order_;
}
uint64_t RingProgress::tail() const
{
    AV_SCOPE("RingProgress::tail");
    return tail_;
}
uint64_t RingProgress::head() const
{
    AV_SCOPE("RingProgress::head");
    return head_;
}

bool RingProgress::invariants_hold() const
{
    AV_SCOPE("RingProgress::invariants_hold");
    return !started_ || consistent(order_, tail_, head_);
}

}  // namespace av
