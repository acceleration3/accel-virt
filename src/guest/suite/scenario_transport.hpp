#ifndef AV_SCENARIO_TRANSPORT_HPP
#define AV_SCENARIO_TRANSPORT_HPP

#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AdapterSearch;
class Kmt;
class ScenarioSteps;

class ScenarioTransport {
public:
    static constexpr uint8_t  ring_order = 12u;
    static constexpr size_t   command_bytes = 1024u;
    static constexpr uint32_t reply_bytes = 4096u;

    ScenarioTransport(Kmt &kmt, const AdapterSearch &search, ScenarioSteps &steps, SubApi sub_api);
    ~ScenarioTransport();
    ScenarioTransport(const ScenarioTransport &) = delete;
    ScenarioTransport &operator=(const ScenarioTransport &) = delete;

    [[nodiscard]] Status open();
    void                 close();
    KmtTransport        &transport();
    GuestConnection     &connection();

private:
    const AdapterSearch &search_;
    ScenarioSteps       &steps_;
    GuestConnection      connection_;
    uint8_t              staging_[command_bytes];
    KmtTransport         transport_;
};

}

#endif
