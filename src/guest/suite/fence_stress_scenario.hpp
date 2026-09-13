/* Feature 039, US3: fences submitted and awaited for as long as the run asks,
 * while the vertical blank's interrupt is enabled and disabled around them -
 * dxgkrnl turns that source off when the last waiter leaves and on when the
 * next arrives, which is the masking a long run has to survive.
 *
 * What this side can see is what the guest can see: every fence it waited for
 * completed, and the worst wait of the run. The interrupt the device injected
 * while nothing was listening is the device's own count (InterruptStressProbe),
 * because only the host can see one that never arrived. */
#ifndef AV_FENCE_STRESS_SCENARIO_HPP
#define AV_FENCE_STRESS_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class GuestSession;
class Kmt;

class FenceStressScenario {
public:
    /* Fences between two vertical blank waits: enough that the blank's
     * interrupt is enabled and disabled many times over a long run, and few
     * enough that a lost wakeup is caught in the batch it happened in. */
    static constexpr uint32_t fences_per_blank = 64u;
    static constexpr uint64_t microseconds_per_second = 1000000u;

    /* `seconds` of 0 runs nothing: the stress is what a long run asks for. */
    FenceStressScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search, uint32_t seconds);

    void run();

    uint32_t completed() const;
    /* The slowest fence of the run, in microseconds. */
    uint64_t worst_wait_microseconds() const;

private:
    [[nodiscard]] Status stress(GuestSession &session, uint64_t address);

    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
    uint32_t             seconds_;
    uint32_t             completed_;
    uint32_t             blanks_;
    uint64_t             worst_;
    uint64_t             frequency_;
};

}  // namespace av

#endif  // AV_FENCE_STRESS_SCENARIO_HPP
