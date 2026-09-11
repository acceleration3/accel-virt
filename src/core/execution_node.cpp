#include "execution_node.hpp"

#include "interrupt_notification.hpp"
#include "invariant_scope.hpp"
#include "spin_lock_guard.hpp"

namespace av {

ExecutionNode::ExecutionNode(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::execution_node), pending_{}, head_(0u), count_(0u),
      last_submitted_(0u), last_completed_(0u), preemption_fence_(0u), preemption_requested_(false) {}

Status ExecutionNode::submit(uint32_t fence)
{
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::submit");
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "submit");
    if (fence <= last_submitted_ || count_ == capacity) return Status::invalid_parameter;
    pending_[(head_ + count_) % capacity] = fence;
    count_++;
    last_submitted_ = fence;
    return Status::ok;
}

Status ExecutionNode::preempt(uint32_t fence)
{
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::preempt");
    SpinLockGuard lock(lock_);
    preemption_fence_ = fence;
    preemption_requested_ = true;
    return Status::ok;
}

bool ExecutionNode::take(uint32_t &fence)
{
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "take");
    if (count_ == 0u) return false;
    fence = pending_[head_];
    head_ = (head_ + 1u) % capacity;
    count_--;
    last_completed_ = fence;
    return true;
}

bool ExecutionNode::take_preemption(uint32_t &fence, uint32_t &last_completed)
{
    SpinLockGuard lock(lock_);
    if (!preemption_requested_) return false;
    preemption_requested_ = false;
    fence = preemption_fence_;
    last_completed = last_completed_;
    return true;
}

/* Reports run without the lock held: DxgkCbSynchronizeExecution raises to
 * device IRQL, and a submission on another processor must not wait for it. */
Status ExecutionNode::complete(InterruptNotification &notification)
{
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::complete");
    uint32_t fence = 0u;
    while (take(fence)) {
        const Status status = notification.report_completed(fence);
        if (status != Status::ok) return status;
    }
    uint32_t last_completed = 0u;
    if (!take_preemption(fence, last_completed)) return Status::ok;
    return notification.report_preempted(fence, last_completed);
}

uint32_t ExecutionNode::current_fence()
{
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::current_fence");
    SpinLockGuard lock(lock_);
    return last_completed_;
}

uint32_t ExecutionNode::pending()
{
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::pending");
    SpinLockGuard lock(lock_);
    return count_;
}

/* Queued fences strictly increase, lie above the last completed fence, and
 * end at the last submitted one. */
bool ExecutionNode::invariants_hold() const
{
    if (count_ > capacity || last_completed_ > last_submitted_) return false;
    uint32_t previous = last_completed_;
    for (uint32_t index = 0u; index < count_; index++) {
        const uint32_t fence = pending_[(head_ + index) % capacity];
        if (fence <= previous) return false;
        previous = fence;
    }
    return count_ == 0u || previous == last_submitted_;
}

}  // namespace av
