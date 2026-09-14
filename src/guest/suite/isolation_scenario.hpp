/* A faulted context changes no other (feature 044, US2): a healthy Vulkan
 * context answers a no-op and makes and destroys an instance; a second
 * context on the same device sends vkCreateInstance with a body too short to
 * decode, which the host refuses: the driver completes it with no reply
 * written and refuses the context's later submissions, and its next send
 * fails; the first context still does all it did before; and a third
 * context made afterwards on the same device answers a no-op. */
#ifndef AV_ISOLATION_SCENARIO_HPP
#define AV_ISOLATION_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class IsolationScenario {
public:
    IsolationScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    IsolationScenario(const IsolationScenario &) = delete;
    IsolationScenario &operator=(const IsolationScenario &) = delete;

    void run();

private:
    /* A no-op, an instance made and destroyed, and a no-op that proves the
     * destroy ran, all on the healthy context. */
    [[nodiscard]] Status exercise_healthy();
    /* Opens `doomed` and sends it the malformed command: ok when the send
     * failed, as a context the host faulted fails it. */
    [[nodiscard]] Status fault(KmtTransport &doomed);

    ScenarioSteps     steps_;
    ScenarioTransport link_;
};

}  // namespace av

#endif  // AV_ISOLATION_SCENARIO_HPP
