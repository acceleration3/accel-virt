/* A command crosses the rvk transport and its answer comes back (features 043
 * and 044): a connection on the adapter the search found, one Vulkan context's
 * transport over it, and a synchronous transport no-op the device answers
 * itself with an empty reply - so it passes whichever executor the host loaded,
 * and its round trip is the transport's own latency. The transport and the
 * connection are torn down whatever failed. */
#ifndef AV_TRANSPORT_SCENARIO_HPP
#define AV_TRANSPORT_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class TransportScenario {
public:
    TransportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

private:
    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
};

}  // namespace av

#endif  // AV_TRANSPORT_SCENARIO_HPP
