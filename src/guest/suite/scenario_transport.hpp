/* The rvk context a scenario runs over (features 043 and 044): a connection on
 * the adapter the search found and one transport of a sub-API on it, set up
 * with each step reported - adapter, connection, initialize - and torn down by
 * close() or the destructor whatever failed. A scenario that needs more
 * contexts opens them on the same connection, as one process does. */
#ifndef AV_SCENARIO_TRANSPORT_HPP
#define AV_SCENARIO_TRANSPORT_HPP

#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class Kmt;
class ScenarioSteps;

class ScenarioTransport {
public:
    /* The smallest ring an RvkContextRecord accepts: one command at a time
     * never needs more. */
    static constexpr uint8_t ring_order = 12u;

    ScenarioTransport(Kmt &kmt, const AdapterSearch &search, ScenarioSteps &steps, SubApi sub_api);
    ~ScenarioTransport();
    ScenarioTransport(const ScenarioTransport &) = delete;
    ScenarioTransport &operator=(const ScenarioTransport &) = delete;

    /* The first step that failed, with nothing after it attempted. */
    [[nodiscard]] Status open();
    /* Tears the transport down, then the connection; nothing if never opened. */
    void                 close();
    KmtTransport        &transport();
    GuestConnection     &connection();

private:
    const AdapterSearch &search_;
    ScenarioSteps       &steps_;
    GuestConnection      connection_;
    KmtTransport         transport_;
};

}  // namespace av

#endif  // AV_SCENARIO_TRANSPORT_HPP
