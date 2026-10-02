#include "miniport/scheduling/hw_queue_submissions.hpp"

#include "miniport/driver/interrupt_notification.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

HwQueueSubmissions::HwQueueSubmissions(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::hw_queue_submissions), queues_{}, fence_ids_{},
      fences_{}, contexts_{}, awaited_{}, count_(0u), written_(false)
      {
          AV_SCOPE("HwQueueSubmissions::HwQueueSubmissions");
      }

Status HwQueueSubmissions::submit(const void *queue, uint64_t fence_id, uint64_t *fence, const void *context,
                                  uint64_t awaited)
{
    AV_SCOPE("HwQueueSubmissions::submit");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::submit");
    SpinLockGuard lock(lock_);
    InvariantScope<HwQueueSubmissions> guard(platform_, *this, "submit");
    if (queue == nullptr || fence == nullptr || count_ == capacity) return Status::invalid_parameter;
    queues_[count_] = queue;
    fence_ids_[count_] = fence_id;
    fences_[count_] = fence;
    contexts_[count_] = context;
    awaited_[count_] = awaited;
    count_++;
    return Status::ok;
}

void HwQueueSubmissions::completed_through(const void *context, uint64_t completed)
{
    AV_SCOPE("HwQueueSubmissions::completed_through");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::completed_through");
    SpinLockGuard lock(lock_);
    InvariantScope<HwQueueSubmissions> guard(platform_, *this, "completed_through");
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        const bool finished = contexts_[index] == context && awaited_[index] <= completed;
        if (finished) {
            platform_.store_atomic(*fences_[index], fence_ids_[index]);
            written_ = true;
            continue;
        }
        queues_[kept] = queues_[index];
        fence_ids_[kept] = fence_ids_[index];
        fences_[kept] = fences_[index];
        contexts_[kept] = contexts_[index];
        awaited_[kept] = awaited_[index];
        kept++;
    }
    count_ = kept;
}

Status HwQueueSubmissions::complete(InterruptNotification &notification)
{
    AV_SCOPE("HwQueueSubmissions::complete");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::complete");
    bool written = false;
    {
        SpinLockGuard lock(lock_);
        InvariantScope<HwQueueSubmissions> guard(platform_, *this, "complete");
        /* A submission that carried no batch has nothing to wait for: the
         * device never heard of it, and what it promised was the fence. */
        uint32_t kept = 0u;
        for (uint32_t index = 0u; index < count_; index++) {
            if (contexts_[index] == nullptr) {
                platform_.store_atomic(*fences_[index], fence_ids_[index]);
                written_ = true;
                continue;
            }
            queues_[kept] = queues_[index];
            fence_ids_[kept] = fence_ids_[index];
            fences_[kept] = fences_[index];
            contexts_[kept] = contexts_[index];
            awaited_[kept] = awaited_[index];
            kept++;
        }
        count_ = kept;
        written = written_;
        written_ = false;
    }
    return written ? notification.report_monitored_fence_signaled(node, engine) : Status::ok;
}

void HwQueueSubmissions::forget(const void *queue)
{
    AV_SCOPE("HwQueueSubmissions::forget");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::forget");
    SpinLockGuard lock(lock_);
    InvariantScope<HwQueueSubmissions> guard(platform_, *this, "forget");
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        if (queues_[index] == queue) continue;
        queues_[kept] = queues_[index];
        fence_ids_[kept] = fence_ids_[index];
        fences_[kept] = fences_[index];
        contexts_[kept] = contexts_[index];
        awaited_[kept] = awaited_[index];
        kept++;
    }
    count_ = kept;
}

void HwQueueSubmissions::abandon()
{
    AV_SCOPE("HwQueueSubmissions::abandon");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::abandon");
    SpinLockGuard lock(lock_);
    count_ = 0u;
}

uint32_t HwQueueSubmissions::in_flight()
{
    AV_SCOPE("HwQueueSubmissions::in_flight");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueSubmissions::in_flight");
    SpinLockGuard lock(lock_);
    return count_;
}

bool HwQueueSubmissions::invariants_hold() const
{
    AV_SCOPE("HwQueueSubmissions::invariants_hold");
    return count_ <= capacity;
}

}  // namespace av
