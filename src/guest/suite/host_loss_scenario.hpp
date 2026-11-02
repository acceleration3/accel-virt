#ifndef AV_HOST_LOSS_SCENARIO_HPP
#define AV_HOST_LOSS_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "guest/transport/guest_connection.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;
class KmtTransport;

class HostLossScenario {
public:
    static constexpr uint32_t idle_seconds = 5u;
    static constexpr uint32_t device_lost_code = 7u;

    HostLossScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    HostLossScenario(const HostLossScenario &) = delete;
    HostLossScenario &operator=(const HostLossScenario &) = delete;

    void run();

private:
    [[nodiscard]] Status draw(KmtTransport &transport, uint32_t device);
    [[nodiscard]] Status make_device(KmtTransport &transport, uint32_t device);
    uint32_t             elapsed_ms(uint64_t started) const;
    [[nodiscard]] Status await_fault(KmtTransport &transport, uint32_t &waited_ms);

    Kmt              &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
    uint8_t           idle_staging_[ScenarioTransport::command_bytes];
    uint8_t           other_staging_[ScenarioTransport::command_bytes];
    uint8_t           next_staging_[ScenarioTransport::command_bytes];
    GuestConnection   other_connection_;
    GuestConnection   next_connection_;
};

}

#endif
