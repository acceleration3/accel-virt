/* What each hardware queue runs on, readable at DISPATCH.
 *
 * `HwQueueTable` owns the queues and answers at PASSIVE, behind a mutex a
 * submission may not take: `DxgkDdiSubmitCommandToHwQueue` arrives at DISPATCH
 * and must reach the queue's context to publish the batch into its ring
 * (feature 049). So the pair is written down when the queue is made and struck
 * out when it is destroyed, and a handle that is in neither is a handle this
 * driver never gave - which is what makes a stale one a refusal rather than a
 * dereference.
 */
#ifndef AV_HW_QUEUE_CONTEXTS_HPP
#define AV_HW_QUEUE_CONTEXTS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Context;
struct HwQueueContextsProbe;

class HwQueueContexts {
public:
    /* One per queue a process may hold, as HwQueueTable's pool allows. */
    static constexpr uint32_t capacity = 64u;

    explicit HwQueueContexts(Platform &platform);
    HwQueueContexts(const HwQueueContexts &) = delete;
    HwQueueContexts &operator=(const HwQueueContexts &) = delete;

    /* PASSIVE, as CreateHwQueue answers. no_space when every slot is taken,
     * invalid_parameter for no queue or no context. */
    [[nodiscard]] Status bind(const void *queue, Context &context);
    /* DISPATCH or below: DestroyHwQueue strikes the queue out. */
    void                 forget(const void *queue);
    /* DISPATCH: the context that queue runs on, null for a handle this driver
     * has none of. */
    Context             *context_of(const void *queue);
    uint32_t             bound();

    /* Every bound slot names both a queue and a context, and no queue is bound
     * twice. */
    bool invariants_hold() const;

private:
    Platform   &platform_;
    SpinLock    lock_;
    const void *queues_[capacity];
    Context    *contexts_[capacity];
    uint32_t    count_;

    friend struct HwQueueContextsProbe;
};

}  // namespace av

#endif  // AV_HW_QUEUE_CONTEXTS_HPP
