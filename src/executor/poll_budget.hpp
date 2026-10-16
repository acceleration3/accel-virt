/* A wait on something the host's GPU or another thread finishes, polled: the
 * first poll at once, then polls back to back for as long as the answer is
 * likely a moment away, then a nap between polls, and a deadline after which
 * the waiter gives up rather than hang its caller - a hung GPU answers "still
 * drawing" forever. Every bounded poll in the executor is one of these. */
#ifndef AV_POLL_BUDGET_HPP
#define AV_POLL_BUDGET_HPP

#include <stdint.h>

namespace av {

class HostClock;

class PollBudget {
public:
    /* Polls back to back for the first `spin_nanoseconds`, then
     * `pause_nanoseconds` apart (0 yields the core between polls); none
     * starts once `deadline_nanoseconds` have passed since the first. */
    PollBudget(HostClock &clock, uint64_t spin_nanoseconds, uint64_t pause_nanoseconds,
               uint64_t deadline_nanoseconds);

    /* Whether another poll may be made, pausing first unless it is the
     * first; false from the deadline on. */
    [[nodiscard]] bool next();
    /* Whether next() said no: the deadline came with the wait unfinished. */
    bool               expired() const;

    bool invariants_hold() const;

private:
    HostClock &clock_;
    uint64_t   spin_nanoseconds_;
    uint64_t   pause_nanoseconds_;
    uint64_t   deadline_nanoseconds_;
    uint64_t   started_;
    bool       polled_;
    bool       expired_;
};

}  // namespace av

#endif  // AV_POLL_BUDGET_HPP
