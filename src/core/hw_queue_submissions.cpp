#include "hw_queue_submissions.hpp"

#include "interrupt_notification.hpp"
#include "invariant_scope.hpp"
#include "spin_lock_guard.hpp"

namespace av {

HwQueueSubmissions::HwQueueSubmissions(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::hw_queue_submissions), queues_{}, fence_ids_{},
      fences_{}, count_(0u) {}

Status HwQueueSubmissions::submit(const void *queue, uint64_t fence_id, uint64_t *fence)
{
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::submit");
    SpinLockGuard lock(lock_);
    InvariantScope<HwQueueSubmissions> guard(platform_, *this, "submit");
    if (queue == nullptr || fence == nullptr || count_ == capacity) return Status::invalid_parameter;
    queues_[count_] = queue;
    fence_ids_[count_] = fence_id;
    fences_[count_] = fence;
    count_++;
    return Status::ok;
}

Status HwQueueSubmissions::complete(InterruptNotification &notification)
{
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::complete");
    uint32_t written = 0u;
    {
        SpinLockGuard lock(lock_);
        InvariantScope<HwQueueSubmissions> guard(platform_, *this, "complete");
        for (; written < count_; written++) platform_.store_atomic(*fences_[written], fence_ids_[written]);
        count_ = 0u;
    }
    return written != 0u ? notification.report_monitored_fence_signaled(node, engine) : Status::ok;
}

void HwQueueSubmissions::forget(const void *queue)
{
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::forget");
    SpinLockGuard lock(lock_);
    InvariantScope<HwQueueSubmissions> guard(platform_, *this, "forget");
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        if (queues_[index] == queue) continue;
        queues_[kept] = queues_[index];
        fence_ids_[kept] = fence_ids_[index];
        fences_[kept] = fences_[index];
        kept++;
    }
    count_ = kept;
}

void HwQueueSubmissions::abandon()
{
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::abandon");
    SpinLockGuard lock(lock_);
    count_ = 0u;
}

uint32_t HwQueueSubmissions::in_flight()
{
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::in_flight");
    SpinLockGuard lock(lock_);
    return count_;
}

bool HwQueueSubmissions::invariants_hold() const { return count_ <= capacity; }

}  // namespace av
