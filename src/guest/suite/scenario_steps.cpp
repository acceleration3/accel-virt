#include "guest/suite/scenario_steps.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

ScenarioSteps::ScenarioSteps(Kmt &kmt, GuestReport &report, const char *scenario)
    : kmt_(kmt), report_(report), scenario_(scenario)
    {
        AV_SCOPE("ScenarioSteps::ScenarioSteps");
    }

Status ScenarioSteps::check(const char *step, Status status)
{
    AV_SCOPE("ScenarioSteps::check");
    report_.check(scenario_, step, status, kmt_.last_code());
    return status;
}

Status ScenarioSteps::check_measured(const char *step, Status status, uint32_t measurement)
{
    AV_SCOPE("ScenarioSteps::check_measured");
    report_.measure(scenario_, step, status, measurement);
    return status;
}

Status ScenarioSteps::check_result(const char *step, int32_t result)
{
    AV_SCOPE("ScenarioSteps::check_result");
    if (result == 0) return check(step, Status::ok);
    return check_measured(step, Status::invalid_state, static_cast<uint32_t>(result));
}

void ScenarioSteps::check_refused(const char *step, Status status)
{
    AV_SCOPE("ScenarioSteps::check_refused");
    report_.check_refused(scenario_, step, status, kmt_.last_code());
}

void ScenarioSteps::check_teardown(GuestSession &session)
{
    AV_SCOPE("ScenarioSteps::check_teardown");
    const Status status = session.tear_down();
    report_.check(scenario_, "teardown", status, session.teardown_code());
}

}  // namespace av
