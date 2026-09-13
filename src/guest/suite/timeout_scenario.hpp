/* Feature 040, US3: what a guest sees when the host stops answering one
 * context. Two devices, each with a context of its own, submit; the host has
 * been told to hold the first (RVK_ECHO_ONLY_NTH with a stall longer than
 * TdrDelay), so dxgkrnl times that node out, resets the engine and reports the
 * context lost - and the claim is that the reset took one context and not the
 * adapter: the other context's work still completes.
 *
 * Nothing but this scenario runs in such a run; every other one would be
 * measuring the stall. */
#ifndef AV_TIMEOUT_SCENARIO_HPP
#define AV_TIMEOUT_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class GuestSession;
class Kmt;

class TimeoutScenario {
public:
    TimeoutScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

private:
    /* A device, an allocation mapped for the GPU and a context: everything the
     * session needs before it can submit. */
    [[nodiscard]] Status prepare(GuestSession &session, uint64_t &address);

    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
};

}  // namespace av

#endif  // AV_TIMEOUT_SCENARIO_HPP
