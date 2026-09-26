/* A call whose only answer is its status, sent without waiting for it: the
 * inner transport is asked to send asynchronously, and the caller is answered
 * a zero status here, never the inner transport's reply. */
#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/unanswered_transport.hpp"
#include "proto/client_transport.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::MockTransport;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::UnansweredTransport;

namespace {

constexpr std::uint32_t command = 0x2002Du;

}  // namespace

TEST(unanswered_transport, a_sync_call_goes_async_and_is_answered_success_here)
{ MockTransport inner;
  /* What a synchronous send of the inner transport would have answered. */
  const std::uint8_t refusal[] = {0x7Fu, 0x01u};
  inner.set_reply(refusal, sizeof refusal);
  UnansweredTransport transport(inner);
  WireWriter writer = transport.request_writer();
  writer.write_pod<std::uint32_t>(0xABCDu);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::ok);
  CHECK_EQ(inner.sends(), 1u);
  CHECK_EQ(inner.last_command(), command);
  CHECK_EQ(inner.last_length(), 4u);
  /* One varint byte of zero, which the generated decoder reads as success. */
  CHECK_EQ(reply.remaining(), 1u);
  CHECK_EQ(reply.read_u8(), 0u);
  CHECK_EQ(reply.remaining(), 0u);
  /* A call ended is the inner transport's to end. */
  transport.end_call();
  CHECK_EQ(inner.end_calls(), 1u); }

TEST(unanswered_transport, a_send_the_inner_transport_fails_is_failed_with_no_answer)
{ MockTransport inner;
  UnansweredTransport transport(inner);
  inner.fail_next();
  WireWriter writer = transport.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::failed);
  CHECK_EQ(reply.remaining(), 0u);
  CHECK_EQ(inner.sends(), 0u); }

TEST(unanswered_transport, an_async_send_and_the_arenas_are_the_inner_transports)
{ MockTransport inner;
  UnansweredTransport transport(inner);
  WireWriter writer = transport.request_writer();
  writer.write_pod<std::uint16_t>(7u);
  CHECK(transport.send_async(command + 1u, writer) == TransportResult::ok);
  CHECK_EQ(inner.sends(), 1u);
  CHECK_EQ(inner.last_command(), command + 1u);
  CHECK_EQ(inner.last_length(), 2u);
  inner.fail_next();
  WireWriter refused = transport.request_writer();
  CHECK(transport.send_async(command, refused) == TransportResult::failed);
  CHECK(&transport.decode_arena() == &inner.decode_arena());
  CHECK(&transport.blobs() == &inner.blobs()); }
