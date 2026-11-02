#ifndef AV_TRANSPORT_SCENARIO_HPP
#define AV_TRANSPORT_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class TransportScenario {
public:
    TransportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search, SubApi sub_api,
                      const char *name);

    TransportScenario(const TransportScenario &) = delete;
    TransportScenario &operator=(const TransportScenario &) = delete;

    void run();

private:
    static constexpr uint32_t warm_rounds = 31u;

    [[nodiscard]] Status timed_nop(uint32_t &answered, uint32_t &microseconds);
    [[nodiscard]] Status first_round_trip();
    void                 warm_round_trips();

    Kmt              &kmt_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
};

}

#endif
