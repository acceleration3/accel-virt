#ifndef AV_ADAPTER_SCENARIO_HPP
#define AV_ADAPTER_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class AdapterScenario {
public:
    static constexpr uint32_t wddm_3_2 = 3200u;

    AdapterScenario(Kmt &kmt, GuestReport &report, AdapterSearch &search);

    void run();

private:
    Kmt           &kmt_;
    AdapterSearch &search_;
    ScenarioSteps  steps_;
};

}

#endif
