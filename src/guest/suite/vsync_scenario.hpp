#ifndef AV_VSYNC_SCENARIO_HPP
#define AV_VSYNC_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class GuestSession;
class Kmt;

class VsyncScenario {
public:
    static constexpr uint32_t expected_hz = 60u;
    static constexpr uint32_t tolerance_percent = 5u;
    static constexpr uint32_t counted_blanks = 600u;
    static constexpr uint32_t source = 0u;

    VsyncScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

    uint32_t rate_centihertz() const;
    uint32_t caught_centihertz() const;
    bool     paced() const;

private:
    [[nodiscard]] Status count(GuestSession &session);

    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
    uint32_t             blanks_;
    uint64_t             first_tick_;
    uint64_t             last_tick_;
    uint64_t             shortest_;
    uint64_t             frequency_;
    uint32_t             expected_centihertz_;
};

}

#endif
