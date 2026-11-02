/* Where the two sides of presented pages sleep (docs/stale-blt-frames.md).
 * A reader sleeps until the pages are settled: no present is being written
 * into them, and the present engine has moved the last one queued into them.
 * A present sleeps until no read is open on the pages it is to write. A
 * present's own thread, a reader's end and the DPC wake the sleepers as each
 * finishes; a sleeper looks again after every wake and after every timeout,
 * since a wake says only that something finished. At most `slots` sleep at
 * once, of either kind; the next is told so rather than made to spin. */
#ifndef AV_PRESENT_WAITERS_HPP
#define AV_PRESENT_WAITERS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/platform/wake_event.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PresentPageView;
class PresentReads;
class PresentWrites;
class StateWriter;
struct PresentWaitersProbe;

class PresentWaiters {
public:
    static constexpr uint32_t slots = 4u;

    explicit PresentWaiters(Platform &platform);
    PresentWaiters(const PresentWaiters &) = delete;
    PresentWaiters &operator=(const PresentWaiters &) = delete;

    /* PASSIVE. Blocks until `writes` is settled - no present open, and the
     * engine's `progress` page (null on a device without an engine) says
     * writes.ticket() has completed, unless it was numbered by an engine of
     * another epoch than `epoch` - or `ticks` (100 ns) pass. `sequence`
     * answers the sequence the read may begin at. ok when settled; not_found
     * when time ran out; no_space when every slot was taken. */
    [[nodiscard]] Status wait_for(PresentWrites &writes, const PresentPageView *progress, uint32_t epoch,
                                  uint64_t ticks, uint64_t &sequence);
    /* PASSIVE. Blocks until `reads` admits a present into `writes` - which
     * opens it - or `ticks` (100 ns) pass. ok when admitted; not_found when
     * time ran out and no_space when every slot was taken, both with `writes`
     * left as it was: the present goes ahead and opens it itself. */
    [[nodiscard]] Status admit_write(PresentReads &reads, PresentWrites &writes, uint64_t ticks);
    /* DISPATCH at most. Every sleeper looks again. */
    void                 wake();
    /* Dispatch at most. How many waits found the pages unsettled and took a
     * slot to wait in, and how many of those ran out of time. */
    uint64_t             waits();
    uint64_t             timeouts();
    /* Dispatch at most. The same for presents: how many found a read open and
     * took a slot to wait in, and how many of those ran out of time. */
    uint64_t             holds();
    uint64_t             hold_timeouts();
    void                 report(StateWriter &writer);

    bool invariants_hold();

private:
    /* Whether `writes` is settled and, if so, the sequence it settled at. */
    [[nodiscard]] bool settled(PresentWrites &writes, const PresentPageView *progress, uint32_t epoch,
                               uint64_t &sequence);
    /* Sleeps in a slot until `done` says so or `ticks` pass, counting into
     * `waits` and `timeouts`. */
    template <typename Done>
    [[nodiscard]] Status sleep_until(uint64_t ticks, uint64_t &waits, uint64_t &timeouts, Done done);
    /* The slot a sleeper may use, marked taken; `slots` when none is free. */
    uint32_t           take();
    void               give(uint32_t slot, uint64_t &waits, uint64_t &timeouts, bool timed_out);

    Platform  &platform_;
    SpinLock   lock_;
    WakeEvent  events_[slots];
    bool       taken_[slots];
    uint64_t   waits_;
    uint64_t   timeouts_;
    uint64_t   holds_;
    uint64_t   hold_timeouts_;

    friend struct PresentWaitersProbe;
};

}  // namespace av

#endif  // AV_PRESENT_WAITERS_HPP
