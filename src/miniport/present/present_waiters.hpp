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

    [[nodiscard]] Status wait_for(PresentWrites &writes, const PresentPageView *progress, uint64_t ticks,
                                  uint64_t &sequence);
    void                 wake();
    uint64_t             waits();
    uint64_t             timeouts();
    void                 report(StateWriter &writer);

    bool invariants_hold();

private:
    [[nodiscard]] bool settled(PresentWrites &writes, const PresentPageView *progress, uint64_t &sequence);
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

}

#endif
