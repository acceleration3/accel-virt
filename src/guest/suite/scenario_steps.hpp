/* The steps of one named scenario as they reach the report: each outcome with
 * the NTSTATUS of the D3DKMT call behind it. */
#ifndef AV_SCENARIO_STEPS_HPP
#define AV_SCENARIO_STEPS_HPP

#include "core/support/status.hpp"

namespace av {

class GuestReport;
class GuestSession;
class Kmt;

class ScenarioSteps {
public:
    ScenarioSteps(Kmt &kmt, GuestReport &report, const char *scenario);

    /* Records a step that must succeed: its status, to chain on. */
    Status check(const char *step, Status status);
    /* Records a step the driver must refuse. */
    void   check_refused(const char *step, Status status);
    /* Tears the session down and records it with its first failure's NTSTATUS. */
    void   check_teardown(GuestSession &session);

private:
    Kmt         &kmt_;
    GuestReport &report_;
    const char  *scenario_;
};

}  // namespace av

#endif  // AV_SCENARIO_STEPS_HPP
