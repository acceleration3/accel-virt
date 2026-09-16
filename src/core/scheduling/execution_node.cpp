#include "core/scheduling/execution_node.hpp"

#include "core/miniport/interrupt_notification.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

ExecutionNode::ExecutionNode(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::execution_node), pending_{}, head_(0u), count_(0u),
      last_submitted_(0u), last_completed_(0u), preemption_fence_(0u), preemption_requested_(false),
      stopped_(false), progress_time_(0u), owners_{}, awaited_{}
      {
          AV_SCOPE("ExecutionNode::ExecutionNode");
      }

Status ExecutionNode::submit(uint32_t fence)
{
    AV_SCOPE("ExecutionNode::submit");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::submit");
    return queue(fence, nullptr, 0u);
}

Status ExecutionNode::submit(uint32_t fence, void *owner, uint64_t awaited)
{
    AV_SCOPE("ExecutionNode::submit");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::submit");
    return owner == nullptr || awaited == 0u ? Status::invalid_parameter : queue(fence, owner, awaited);
}

Status ExecutionNode::queue(uint32_t fence, void *owner, uint64_t awaited)
{
    AV_SCOPE("ExecutionNode::queue");
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "submit");
    if (stopped_) return Status::invalid_state;
    if (fence <= last_submitted_ || count_ == capacity) return Status::invalid_parameter;
    if (count_ == 0u) progress_time_ = platform_.interrupt_time();
    const uint32_t tail = (head_ + count_) % capacity;
    pending_[tail] = fence;
    owners_[tail] = owner;
    awaited_[tail] = awaited;
    count_++;
    last_submitted_ = fence;
    return Status::ok;
}

void ExecutionNode::finish(const void *owner, uint64_t completed)
{
    AV_SCOPE("ExecutionNode::finish");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::finish");
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "finish");
    if (owner == nullptr) return;
    for (uint32_t index = 0u; index < count_; index++) {
        const uint32_t slot = (head_ + index) % capacity;
        if (owners_[slot] == owner && awaited_[slot] <= completed) awaited_[slot] = 0u;
    }
}

Status ExecutionNode::preempt(uint32_t fence)
{
    AV_SCOPE("ExecutionNode::preempt");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::preempt");
    SpinLockGuard lock(lock_);
    preemption_fence_ = fence;
    preemption_requested_ = true;
    return Status::ok;
}

bool ExecutionNode::take(uint32_t &fence)
{
    AV_SCOPE("ExecutionNode::take");
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "take");
    if (count_ == 0u || awaited_[head_] != 0u) return false;
    fence = pending_[head_];
    head_ = (head_ + 1u) % capacity;
    count_--;
    last_completed_ = fence;
    progress_time_ = platform_.interrupt_time();
    return true;
}

bool ExecutionNode::take_preemption(uint32_t &fence, uint32_t &last_completed)
{
    AV_SCOPE("ExecutionNode::take_preemption");
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
    AV_SCOPE("ExecutionNode::complete");
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
    AV_SCOPE("ExecutionNode::current_fence");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::current_fence");
    SpinLockGuard lock(lock_);
    return last_completed_;
}

uint32_t ExecutionNode::last_submitted()
{
    AV_SCOPE("ExecutionNode::last_submitted");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::last_submitted");
    SpinLockGuard lock(lock_);
    return last_submitted_;
}

uint64_t ExecutionNode::progress_time()
{
    AV_SCOPE("ExecutionNode::progress_time");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::progress_time");
    SpinLockGuard lock(lock_);
    return progress_time_;
}

void ExecutionNode::reset()
{
    AV_SCOPE("ExecutionNode::reset");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::reset");
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "reset");
    head_ = 0u;
    count_ = 0u;
    preemption_requested_ = false;
    stopped_ = true;
}

void ExecutionNode::restart()
{
    AV_SCOPE("ExecutionNode::restart");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::restart");
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "restart");
    stopped_ = false;
}

uint32_t ExecutionNode::abort_pending()
{
    AV_SCOPE("ExecutionNode::abort_pending");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::abort_pending");
    SpinLockGuard lock(lock_);
    InvariantScope<ExecutionNode> guard(platform_, *this, "abort_pending");
    const uint32_t executing = count_ != 0u ? pending_[head_] : last_completed_;
    head_ = 0u;
    count_ = 0u;
    preemption_requested_ = false;
    return executing;
}

uint32_t ExecutionNode::pending()
{
    AV_SCOPE("ExecutionNode::pending");
    platform_.require_irql_at_most(Irql::dispatch, "ExecutionNode::pending");
    SpinLockGuard lock(lock_);
    return count_;
}

/* Queued fences strictly increase, lie above the last completed fence, and
 * end at the last submitted one; a packet awaiting its host has an owner to
 * finish it. */
bool ExecutionNode::invariants_hold() const
{
    AV_SCOPE("ExecutionNode::invariants_hold");
    if (count_ > capacity || last_completed_ > last_submitted_ || (stopped_ && count_ != 0u)) return false;
    uint32_t previous = last_completed_;
    for (uint32_t index = 0u; index < count_; index++) {
        const uint32_t slot = (head_ + index) % capacity;
        const uint32_t fence = pending_[slot];
        if (fence <= previous || (awaited_[slot] != 0u && owners_[slot] == nullptr)) return false;
        previous = fence;
    }
    return count_ == 0u || previous == last_submitted_;
}

}  // namespace av
