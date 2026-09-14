/* The one node's packets from submission to completion. SubmitCommand queues a
 * fence; the DPC completes everything queued, in order, and reports it - a
 * loopback engine until the device executes packets (feature 030). Reachable
 * at DISPATCH from SubmitCommand, PreemptCommand and the DPC at once, so the
 * state is under a spin lock and nothing here allocates or blocks. */
#ifndef AV_EXECUTION_NODE_HPP
#define AV_EXECUTION_NODE_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class InterruptNotification;
struct ExecutionNodeProbe;

class ExecutionNode {
public:
    /* More than VidSch queues to a node before its DPC runs. */
    static constexpr uint32_t capacity = 64u;

    explicit ExecutionNode(Platform &platform);
    ExecutionNode(const ExecutionNode &) = delete;
    ExecutionNode &operator=(const ExecutionNode &) = delete;

    /* invalid_parameter for a fence not above the last submitted, or a full queue. */
    [[nodiscard]] Status submit(uint32_t fence);
    /* The same, remembering whose submission it is: SubmitCommandVirtual names
     * a context, and a reset has to say whose work it threw away. */
    [[nodiscard]] Status submit(uint32_t fence, void *owner);
    /* The same, for a packet the host runs: it completes only once `owner`
     * has been finished through the host fence `awaited`, and holds every
     * packet queued behind it until then (feature 043). */
    [[nodiscard]] Status submit(uint32_t fence, void *owner, uint64_t awaited);
    /* `owner`'s host has completed every fence up to `completed`. */
    void                 finish(const void *owner, uint64_t completed);
    /* The latest request wins; reported at the next completion. */
    [[nodiscard]] Status preempt(uint32_t fence);
    /* Completes and reports every queued fence, then any preemption; stops at
     * the first report dxgkrnl refuses. */
    [[nodiscard]] Status complete(InterruptNotification &notification);
    uint32_t             current_fence();
    uint32_t             pending();
    uint32_t             last_submitted();
    /* Interrupt time of the last submission into an idle queue or completion. */
    uint64_t             progress_time();
    /* ResetFromTimeout: VidSch aborts every packet itself, so the queue is
     * dropped without a report, and submissions are refused until restart. */
    void                 reset();
    void                 restart();
    /* ResetEngine: drops the queue without a report, leaving the node running,
     * and answers the fence that was executing - the last completed one when
     * nothing was queued. */
    uint32_t             abort_pending();
    /* The same, with the context that submitted the fence that was executing,
     * or null for a submission that named none. */
    uint32_t             abort_pending(void *&owner);

    bool invariants_hold() const;

private:
    [[nodiscard]] Status queue(uint32_t fence, void *owner, uint64_t awaited);
    /* Takes the next queued fence into `fence`, marking it completed; nothing
     * while the head still awaits its host. */
    bool take(uint32_t &fence);
    bool take_preemption(uint32_t &fence, uint32_t &last_completed);

    Platform &platform_;
    SpinLock  lock_;
    uint32_t  pending_[capacity];
    uint32_t  head_;
    uint32_t  count_;
    uint32_t  last_submitted_;
    uint32_t  last_completed_;
    uint32_t  preemption_fence_;
    bool      preemption_requested_;
    bool      stopped_;
    uint64_t  progress_time_;
    void     *owners_[capacity];
    /* The host fence each packet waits for; 0 once nothing is awaited. */
    uint64_t  awaited_[capacity];

    friend struct ExecutionNodeProbe;
};

}  // namespace av

#endif  // AV_EXECUTION_NODE_HPP
