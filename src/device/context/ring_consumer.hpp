#ifndef AV_RING_CONSUMER_HPP
#define AV_RING_CONSUMER_HPP

#include "core/support/status.hpp"
#include "device/context/held_completions.hpp"
#include "device/context/ring_window.hpp"
#include "device/context/submission_watchdog.hpp"
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

    [[nodiscard]] Status prepare(const RingWindow &ring);
    void                 withdraw();
    void                 attach();
    void                 detach();
    void                 service();
    void                 abandon();

    bool     attached() const;
    bool     faulted() const;
    uint64_t head() const;
    uint64_t earliest_pending() const;
    bool     has_work() const;
    void     announce_awake(bool awake);
    bool     poll();
    void     order_with(D3dOrdering &ordering);
    uint64_t last_fence() const;
    uint64_t batches() const;

    bool invariants_hold() const;

private:
    void                 serve();
    [[nodiscard]] Status consume_next();
    void                 complete_or_hold(uint64_t fence);
    bool                 settle();
    void                 note_clock(uint64_t published_at);
    void                 note_work();
    void                 fault(FaultCode code, const char *reason, uint64_t value);

    Host          &host_;
    FenceReporter &fences_;
    BatchExecutor &executor_;
    RingWindow     ring_;
    RingProgress progress_;
    HeldCompletions held_;
    SubmissionWatchdog watchdog_;
    uint64_t     head_;
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

}

#endif
