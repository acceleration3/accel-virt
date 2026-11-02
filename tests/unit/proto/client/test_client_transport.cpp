#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::MockTransport;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;

TEST(client_transport, a_writer_is_lent_and_what_is_sent_is_recorded)
{ MockTransport transport;
  av::ClientTransport &t = transport;
  WireWriter writer = t.request_writer();
  writer.write_u32(0xABCDu);
  writer.write_u32(0x1234u);
  CHECK(t.send_async(0x10007u, writer) == TransportResult::ok);
  CHECK_EQ(transport.sends(), 1u);
  CHECK_EQ(transport.last_command(), 0x10007u);
  CHECK_EQ(transport.last_length(), 8u); }

TEST(client_transport, a_sync_send_reads_the_reply_back)
{ MockTransport transport;
  const std::uint8_t answer[] = {1, 2, 3, 4, 5, 6, 7, 8};
  transport.set_reply(answer, sizeof answer);
  av::ClientTransport &t = transport;
  WireWriter writer = t.request_writer();
  writer.write_u32(9u);
  WireReader reply(nullptr, 0u);
  CHECK(t.send_sync(0x10008u, writer, reply) == TransportResult::ok);
  CHECK_EQ(reply.read_u32(), 0x04030201u);
  CHECK_STATUS(reply.status(), av::Status::ok); }

TEST(client_transport, an_overflowed_writer_is_refused_not_sent)
{ MockTransport transport;
  av::ClientTransport &t = transport;
  WireWriter writer = t.request_writer();
  writer.write_zeros(MockTransport::slot_bytes + 1u);
  CHECK_STATUS(writer.status(), av::Status::too_large);
  CHECK(t.send_async(0x10009u, writer) == TransportResult::failed);
  WireReader reply(nullptr, 0u);
  CHECK(t.send_sync(0x10009u, writer, reply) == TransportResult::failed);
  CHECK_EQ(transport.sends(), 0u); }

TEST(client_transport, a_failed_send_answers_failed)
{ MockTransport transport;
  av::ClientTransport &t = transport;
  transport.fail_next();
  WireWriter writer = t.request_writer();
  writer.write_u32(1u);
  WireReader reply(nullptr, 0u);
  CHECK(t.send_sync(0x1000Au, writer, reply) == TransportResult::failed);
  CHECK_EQ(transport.sends(), 0u); }

TEST(client_transport, the_decode_arena_refuses)
{ MockTransport transport;
  av::ClientTransport &t = transport;
  CHECK(t.decode_arena().allocate<std::uint8_t>(1u) == nullptr);
  CHECK(!t.blobs().ready()); }
