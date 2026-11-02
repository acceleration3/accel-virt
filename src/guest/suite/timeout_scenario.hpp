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
    [[nodiscard]] Status prepare(GuestSession &session, uint64_t &address);

    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
};

}

#endif
