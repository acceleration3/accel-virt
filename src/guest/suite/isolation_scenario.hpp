#ifndef AV_ISOLATION_SCENARIO_HPP
#define AV_ISOLATION_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"

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
    [[nodiscard]] Status exercise_healthy();
    [[nodiscard]] Status fault(KmtTransport &doomed);

    ScenarioSteps     steps_;
    ScenarioTransport link_;
    vkproto::HandleIdentity handles_;
    uint8_t           staging_[ScenarioTransport::command_bytes];
};

}

#endif
