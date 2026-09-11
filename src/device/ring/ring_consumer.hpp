/* Consumes one context's ring on its worker (docs/protocol.md): every complete
 * batch between the device's head and the tail the guest published, head
 * published after each. The guest can rewrite any byte at any moment, so each
 * header snapshot is judged against what the device has already seen, each
 * batch header is copied before it is read, and the first ring no valid
 * producer could write stops the context for good, logged once.
 *
 * The setup prepares it on the main loop; attach runs on the worker, reached
 * through the work queue, so the worker only reads the ring the queue's lock
 * has published to it. */
#ifndef AV_RING_CONSUMER_HPP
#define AV_RING_CONSUMER_HPP

#include "core/support/status.hpp"
#include "device/ring/ring_window.hpp"
#include "proto/submission/ring_progress.hpp"

#include <stdint.h>

namespace av {

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

    bool     attached() const;
    bool     stopped() const;
    uint64_t head() const;
    uint64_t last_fence() const;
    uint64_t batches() const;

    bool invariants_hold() const;

private:
    /* ok for a consumed batch; no_space until the guest publishes the rest;
     * anything else has stopped the context. */
    [[nodiscard]] Status consume_next();
    void                 stop(const char *reason, uint64_t value);

    Host          &host_;
    FenceReporter &fences_;
    BatchExecutor &executor_;
    RingWindow     ring_;
    RingProgress progress_;
    uint64_t     head_;
    uint64_t     last_fence_;
    uint64_t     batches_;
    bool         prepared_;
    bool         attached_;
    bool         stopped_;

    friend struct RingConsumerProbe;
};

}  // namespace av

#endif  // AV_RING_CONSUMER_HPP
