/* Where a reader of presented pages sleeps until they are settled
 * (docs/stale-blt-frames.md): no present is being written into them, and the
 * present engine has moved the last one queued into them. A present's own
 * thread and the DPC wake the sleepers as each finishes; a sleeper looks at
 * the pages again after every wake and after every timeout, since a wake
 * says only that something finished. At most `slots` readers sleep at once;
 * the next is told so rather than made to spin. */
#ifndef AV_PRESENT_WAITERS_HPP
#define AV_PRESENT_WAITERS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/platform/wake_event.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PresentPageView;
class PresentWrites;
class StateWriter;
struct PresentWaitersProbe;

class PresentWaiters {
public:
    static constexpr uint32_t slots = 4u;

    explicit PresentWaiters(Platform &platform);
    PresentWaiters(const PresentWaiters &) = delete;
    PresentWaiters &operator=(const PresentWaiters &) = delete;

    /* PASSIVE. Blocks until `writes` is settled - an even sequence, and the
     * engine's `progress` page (null on a device without an engine) says
     * writes.ticket() has completed - or `ticks` (100 ns) pass. `sequence`
     * answers the even count the read may begin at. ok when settled;
     * not_found when time ran out; no_space when every slot was taken. */
    [[nodiscard]] Status wait_for(PresentWrites &writes, const PresentPageView *progress, uint64_t ticks,
                                  uint64_t &sequence);
    /* DISPATCH at most. Every sleeper looks again. */
    void                 wake();
    /* Dispatch at most. How many waits found the pages unsettled and took a
     * slot to wait in, and how many of those ran out of time. */
    uint64_t             waits();
    uint64_t             timeouts();
    void                 report(StateWriter &writer);

    bool invariants_hold();

private:
    /* Whether `writes` is settled and, if so, the sequence it settled at. */
    [[nodiscard]] bool settled(PresentWrites &writes, const PresentPageView *progress, uint64_t &sequence);
    /* The slot a sleeper may use, marked taken; `slots` when none is free. */
    uint32_t           take();
    void               give(uint32_t slot, bool timed_out);

    Platform  &platform_;
    SpinLock   lock_;
    WakeEvent  events_[slots];
    bool       taken_[slots];
    uint64_t   waits_;
    uint64_t   timeouts_;

    friend struct PresentWaitersProbe;
};

}  // namespace av

#endif  // AV_PRESENT_WAITERS_HPP
