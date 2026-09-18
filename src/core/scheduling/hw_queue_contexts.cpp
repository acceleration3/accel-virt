#include "core/scheduling/hw_queue_contexts.hpp"

#include "core/platform/spin_lock_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

HwQueueContexts::HwQueueContexts(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::hw_queue_contexts), queues_{}, contexts_{}, count_(0u)
{
    AV_SCOPE("HwQueueContexts::HwQueueContexts");
}

Status HwQueueContexts::bind(const void *queue, Context &context)
{
    AV_SCOPE("HwQueueContexts::bind");
    platform_.require_irql_at_most(Irql::passive, "HwQueueContexts::bind");
    SpinLockGuard                   lock(lock_);
    InvariantScope<HwQueueContexts> guard(platform_, *this, "bind");
    if (queue == nullptr || count_ == capacity) return queue == nullptr ? Status::invalid_parameter : Status::no_space;
    queues_[count_] = queue;
    contexts_[count_] = &context;
    count_++;
    return Status::ok;
}

void HwQueueContexts::forget(const void *queue)
{
    AV_SCOPE("HwQueueContexts::forget");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueContexts::forget");
    SpinLockGuard                   lock(lock_);
    InvariantScope<HwQueueContexts> guard(platform_, *this, "forget");
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        if (queues_[index] == queue) continue;
        queues_[kept] = queues_[index];
        contexts_[kept] = contexts_[index];
        kept++;
    }
    count_ = kept;
}

Context *HwQueueContexts::context_of(const void *queue)
{
    AV_SCOPE("HwQueueContexts::context_of");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueContexts::context_of");
    SpinLockGuard lock(lock_);
    for (uint32_t index = 0u; index < count_; index++)
        if (queues_[index] == queue && queue != nullptr) return contexts_[index];
    return nullptr;
}

uint32_t HwQueueContexts::bound()
{
    AV_SCOPE("HwQueueContexts::bound");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueueContexts::bound");
    SpinLockGuard lock(lock_);
    return count_;
}

bool HwQueueContexts::invariants_hold() const
{
    AV_SCOPE("HwQueueContexts::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (queues_[index] == nullptr || contexts_[index] == nullptr) return false;
        for (uint32_t other = index + 1u; other < count_; other++)
            if (queues_[index] == queues_[other]) return false;
    }
    return true;
}

}  // namespace av
