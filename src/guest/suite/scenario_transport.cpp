#include "guest/suite/scenario_transport.hpp"

#include "guest/suite/adapter_search.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

ScenarioTransport::ScenarioTransport(Kmt &kmt, const AdapterSearch &search, ScenarioSteps &steps, SubApi sub_api)
    : search_(search), steps_(steps), connection_(kmt), staging_{},
      transport_(connection_, sub_api, ring_order, reply_bytes, staging_, sizeof staging_) {}

ScenarioTransport::~ScenarioTransport() { close(); }

Status ScenarioTransport::open()
{
    const Status found = steps_.check("adapter", search_.found() ? Status::ok : Status::not_found);
    if (found != Status::ok) return found;
    const Status connected = steps_.check("connection", connection_.open(search_.adapter()));
    if (connected != Status::ok) return connected;
    return steps_.check("initialize", transport_.initialize());
}

void ScenarioTransport::close()
{
    transport_.teardown();
    connection_.close();
}

KmtTransport    &ScenarioTransport::transport() { return transport_; }
GuestConnection &ScenarioTransport::connection() { return connection_; }

}  // namespace av
