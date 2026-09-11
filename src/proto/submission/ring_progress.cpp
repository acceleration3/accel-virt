#include "proto/submission/ring_progress.hpp"

#include "proto/submission/ring_header.hpp"

namespace av {

RingProgress::RingProgress() : started_(false), order_(0u), tail_(0u), head_(0u) {}

bool RingProgress::consistent(uint8_t order, uint64_t tail, uint64_t head) const
{
    if (order < RingHeader::lowest_order || order > RingHeader::highest_order) return false;
    return head <= tail && tail - head <= (uint64_t{1} << order);
}

Status RingProgress::observe(const RingHeader &snapshot)
{
    if (!consistent(snapshot.order(), snapshot.tail(), snapshot.head())) return Status::invalid_state;
    if (started_ && (snapshot.order() != order_ || snapshot.head() < head_ || snapshot.tail() < tail_))
        return Status::invalid_state;
    started_ = true;
    order_ = snapshot.order();
    tail_ = snapshot.tail();
    head_ = snapshot.head();
    return Status::ok;
}

bool     RingProgress::started() const { return started_; }
uint8_t  RingProgress::order() const   { return order_; }
uint64_t RingProgress::tail() const    { return tail_; }
uint64_t RingProgress::head() const    { return head_; }

bool RingProgress::invariants_hold() const
{
    return !started_ || consistent(order_, tail_, head_);
}

}  // namespace av
