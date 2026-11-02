#ifndef AV_SCENARIO_STEPS_HPP
#define AV_SCENARIO_STEPS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestReport;
class GuestSession;
class Kmt;

class ScenarioSteps {
public:
    ScenarioSteps(Kmt &kmt, GuestReport &report, const char *scenario);

    Status check(const char *step, Status status);
    Status check_kernel(const char *step, Status status, uint32_t code);
    Status check_measured(const char *step, Status status, uint32_t measurement);
    Status check_result(const char *step, int32_t result);
    void   note(const char *key, const char *text);
    void   check_refused(const char *step, Status status);
    void   check_teardown(GuestSession &session);

private:
    Kmt         &kmt_;
    GuestReport &report_;
    const char  *scenario_;
};

}

#endif
