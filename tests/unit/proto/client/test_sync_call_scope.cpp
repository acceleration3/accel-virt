#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::MockTransport;
using av::SyncCallScope;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::uint32_t command = 0x10008u;

std::uint32_t answer_with_the_call_open(MockTransport &transport)
{
    WireWriter writer = transport.request_writer();
    const SyncCallScope call(transport);
    WireReader reply(nullptr, 0u);
    if (transport.send_sync(command, writer, reply) != TransportResult::ok) return 0u;
    return transport.end_calls() == 0u ? reply.read_u32() : 0u;
}

}

TEST(sync_call_scope, the_call_is_open_while_the_reply_is_read_and_ends_once_after)
{ MockTransport transport;
  const std::uint8_t answer[] = {7, 0, 0, 0};
  transport.set_reply(answer, sizeof answer);
  CHECK_EQ(answer_with_the_call_open(transport), 7u);
  CHECK_EQ(transport.end_calls(), 1u); }

TEST(sync_call_scope, a_refused_send_still_ends_the_call_once)
{ MockTransport transport;
  transport.fail_next();
  CHECK_EQ(answer_with_the_call_open(transport), 0u);
  CHECK_EQ(transport.sends(), 0u);
  CHECK_EQ(transport.end_calls(), 1u); }
