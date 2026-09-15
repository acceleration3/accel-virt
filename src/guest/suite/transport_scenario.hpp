/* A command crosses the rvk transport and its answer comes back (features 043
 * and 044): a connection on the adapter the search found, one context's
 * transport of the sub-API asked for - its setup is the host opening that API's
 * backend on its GPU - and a synchronous transport no-op the device answers
 * itself with an empty reply - so it passes whichever executor the host loaded,
 * and its round trip is the transport's own latency - then a second, whose
 * round trip is a command's once the host's backend is open. The transport and the
 * connection are torn down whatever failed. */
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
    /* `name` is the scenario the report's lines carry. */
    TransportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search, SubApi sub_api,
                      const char *name);

    TransportScenario(const TransportScenario &) = delete;
    TransportScenario &operator=(const TransportScenario &) = delete;

    void run();

private:
    /* One synchronous no-op, judged as `nop` and timed as `latency`. */
    [[nodiscard]] Status round_trip(const char *nop, const char *latency);

    Kmt              &kmt_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
};

}  // namespace av

#endif  // AV_TRANSPORT_SCENARIO_HPP
