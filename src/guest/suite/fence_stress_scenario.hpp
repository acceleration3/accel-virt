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
    static constexpr uint32_t fences_per_blank = 64u;
    static constexpr uint64_t microseconds_per_second = 1000000u;

    FenceStressScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search, uint32_t seconds);

    void run();

    uint32_t completed() const;
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

}

#endif
