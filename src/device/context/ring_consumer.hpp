/* Consumes one context's ring on its worker (docs/protocol.md): every complete
 * batch between the device's head and the tail the guest published, executed,
 * then head published and the fence completed. The guest can rewrite any byte
 * at any moment, so each header snapshot is judged against what the device has
 * already seen, each batch header is copied before it is read, and the first
 * ring no valid producer could write, or batch that cannot run, faults the
 * context for good: logged once, the executor destroyed, the fault reported.
 *
 * The setup prepares it on the main loop; attach runs on the worker, reached
 * through the work queue, so the worker only reads the ring the queue's lock
 * has published to it. */
#ifndef AV_RING_CONSUMER_HPP
#define AV_RING_CONSUMER_HPP

#include "core/support/status.hpp"
#include "device/context/held_completions.hpp"
#include "device/context/ring_window.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/ring_progress.hpp"

#include <stdint.h>

namespace av {

class D3dOrdering;

class BatchExecutor;
class FenceReporter;
class Host;
struct RingConsumerProbe;

class RingConsumer {
public:
    RingConsumer(Host &host, FenceReporter &fences, BatchExecutor &batches);
    RingConsumer(const RingConsumer &) = delete;
    RingConsumer &operator=(const RingConsumer &) = delete;

    /* Main loop. invalid_parameter for a window its blob cannot hold, or a
     * header that is malformed, of another order, with head other than 0 or a
     * tail more than a ring ahead; invalid_state once prepared. */
    [[nodiscard]] Status prepare(const RingWindow &ring);
    /* Main loop: forgets a prepared ring whose attachment was never posted. */
    void                 withdraw();
    /* Worker: serving begins, if prepared, with the context's executor
     * created; a context whose executor cannot be created stops. */
    void                 attach();
    /* Worker, before it exits: the context's executor is destroyed. */
    void                 detach();
    /* Worker: every complete batch the guest has published is executed and
     * completed, and the guest interrupted once if any was. */
    void                 service();
    /* Any thread, as the worker is stopped: no batch begins after the one
     * running, which stops at its next command, and nothing faults for it -
     * the context is going, and nobody waits on what it held. The next
     * attach serves again. */
    void                 abandon();

    bool     attached() const;
    bool     faulted() const;
    uint64_t head() const;
    /* From another worker: when the guest published the oldest batch this ring
     * holds and has not begun to run, or UINT64_MAX for none. One running is
     * not waited for: until it has recorded its last command it holds its
     * API's device lock, which orders a later batch behind it anyway, and once
     * it has, all it can still be doing is waiting on the GPU for an answer -
     * a read back - that nothing published after it depends on. */
    uint64_t earliest_pending() const;
    /* Any thread: the guest has published a batch not yet taken. */
    bool     has_work() const;
    /* Worker thread: whether it is watching the ring, for the process to read. */
    void     announce_awake(bool awake);
    /* Worker thread: the executor publishes what the guest will read, and
     * batches whose GPU work has finished complete; true while the executor
     * still waits on the GPU to publish. */
    bool     poll();
    /* The Direct3D streams whose batches this one's follow. */
    void     order_with(D3dOrdering &ordering);
    uint64_t last_fence() const;
    uint64_t batches() const;

    bool invariants_hold() const;

private:
    /* ok for a consumed batch; no_space until the guest publishes the rest;
     * anything else has faulted the context. */
    void                 serve();
    [[nodiscard]] Status consume_next();
    /* A batch that left GPU work unfinished completes when the GPU gets
     * there, and every batch after it no sooner. */
    void                 complete_or_hold(uint64_t fence);
    /* Completes what the GPU has reached; whether anything was. */
    bool                 settle();
    void                 note_clock(uint64_t published_at);
    /* Tells the pulse whether the worker is at work for the guest: running a
     * batch, which can block in a host call for seconds, or keeping watch on
     * results the host GPU has yet to finish. */
    void                 note_work();
    void                 fault(FaultCode code, const char *reason, uint64_t value);

    Host          &host_;
    FenceReporter &fences_;
    BatchExecutor &executor_;
    RingWindow     ring_;
    RingProgress progress_;
    HeldCompletions held_;
    uint64_t     head_;
    /* Where the batch running ends, 0 while none runs; another worker's
     * earliest_pending looks past it. */
    uint64_t     running_end_;
    uint64_t     last_fence_;
    uint64_t     batches_;
    D3dOrdering *ordering_;
    bool         clock_pairs_;
    bool         prepared_;
    bool         attached_;
    bool         faulted_;
    bool         serving_;
    bool         publishing_;

    friend struct RingConsumerProbe;
};

}  // namespace av

#endif  // AV_RING_CONSUMER_HPP
