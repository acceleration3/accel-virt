/* A command crosses the rvk transport and its answer comes back (feature 043):
 * a connection on the adapter the search found, one Vulkan context's transport
 * over it, a synchronous echo whose reply must hold exactly the payload sent.
 * The host of a guest run answers with the echo executor. The transport and
 * the connection are torn down whatever failed. */
#ifndef AV_TRANSPORT_SCENARIO_HPP
#define AV_TRANSPORT_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;
class WireReader;

class TransportScenario {
public:
    /* No byte of it is zero, so a reply slot the device never wrote cannot
     * pass for its echo. */
    static constexpr uint64_t echo_payload = 0x5256544B52564B21ull;
    static constexpr uint32_t payload_bytes = 8u;

    TransportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

private:
    /* ok when the answer is the payload and nothing else; the bytes of it that
     * matched are what the report carries. */
    [[nodiscard]] Status judge(WireReader &answer);

    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
    uint32_t             matched_;
};

}  // namespace av

#endif  // AV_TRANSPORT_SCENARIO_HPP
