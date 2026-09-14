#include "guest/suite/transport_scenario.hpp"

#include "guest/suite/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/client_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

/* The command the host's echo executor answers with its own payload
 * (tests/qemu/echo_executor.cpp). */
constexpr uint32_t echo_command = 0x10001u;
/* The smallest ring an RvkContextRecord accepts: one echo needs no more. */
constexpr uint8_t  ring_order = 12u;
constexpr uint32_t bits_per_byte = 8u;
constexpr uint64_t byte_mask = 0xFFu;

}  // namespace

TransportScenario::TransportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "transport"), matched_(0u) {}

void TransportScenario::run()
{
    matched_ = 0u;
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestConnection connection(kmt_);
    KmtTransport    transport(connection, SubApi::vulkan, ring_order);
    WireReader      answer(nullptr, 0u);
    Status          status = steps_.check("connection", connection.open(search_.adapter()));
    if (status == Status::ok) status = steps_.check("initialize", transport.initialize());
    if (status == Status::ok) {
        WireWriter request = transport.request_writer();
        request.write_u64(echo_payload);
        const TransportResult sent = transport.send_sync(echo_command, request, answer);
        /* A failed call has lost the context, which is the state a device
         * reset leaves it in. */
        status = steps_.check("send_sync", sent == TransportResult::ok ? Status::ok : Status::invalid_state);
    }
    if (status == Status::ok) static_cast<void>(steps_.check_measured("reply", judge(answer), matched_));
    transport.teardown();
    connection.close();
}

Status TransportScenario::judge(WireReader &answer)
{
    const bool     whole = answer.remaining() == payload_bytes;
    const uint64_t echoed = answer.read_u64();
    for (uint32_t index = 0u; index < payload_bytes; index++) {
        const uint32_t shift = index * bits_per_byte;
        matched_ += static_cast<uint32_t>(((echoed >> shift) & byte_mask) == ((echo_payload >> shift) & byte_mask));
    }
    return whole && matched_ == payload_bytes ? Status::ok : Status::malformed;
}

}  // namespace av
