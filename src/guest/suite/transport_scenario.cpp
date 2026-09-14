#include "guest/suite/transport_scenario.hpp"

#include "guest/kmt/kmt.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/client_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

/* The smallest ring an RvkContextRecord accepts: one no-op needs no more. */
constexpr uint8_t  ring_order = 12u;
constexpr uint64_t microseconds_per_second = 1000000u;

}  // namespace

TransportScenario::TransportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "transport") {}

void TransportScenario::run()
{
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestConnection connection(kmt_);
    KmtTransport    transport(connection, SubApi::vulkan, ring_order);
    Status          status = steps_.check("connection", connection.open(search_.adapter()));
    if (status == Status::ok) status = steps_.check("initialize", transport.initialize());
    if (status == Status::ok) {
        WireWriter     request = transport.request_writer();
        WireReader     answer(nullptr, 0u);
        uint64_t       frequency = 0u;
        const uint64_t sent_at = kmt_.performance_counter(frequency);
        const TransportResult sent = transport.send_sync(CommandHeader::nop_id, request, answer);
        const uint64_t answered_at = kmt_.performance_counter(frequency);
        /* A failed call has lost the context, which is the state a device
         * reset leaves it in; an answer with anything in it is not a no-op's. */
        const Status nop = sent != TransportResult::ok ? Status::invalid_state
                           : answer.remaining() != 0u  ? Status::malformed
                                                       : Status::ok;
        status = steps_.check_measured("nop", nop, static_cast<uint32_t>(answer.remaining()));
        const uint64_t elapsed = answered_at - sent_at;
        const uint64_t microseconds = frequency == 0u ? 0u : elapsed * microseconds_per_second / frequency;
        /* Reported, never judged: SC-002's threshold comes from a live run. */
        if (status == Status::ok)
            static_cast<void>(steps_.check_measured("latency", Status::ok, static_cast<uint32_t>(microseconds)));
    }
    transport.teardown();
    connection.close();
}

}  // namespace av
