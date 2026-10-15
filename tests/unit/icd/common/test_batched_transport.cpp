/* The commands a transport would send a batch each, framed into one block of
 * the caller's and sent together: when the room left could not take another,
 * when a command wants an answer - it is the batch's last - and when the owner
 * flushes. What the device finds is read back off MockKmt's ring, where every
 * command of a batch is journalled. */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/batched_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>

using av::CommandHeader;
using av::GuestConnection;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::BatchedTransport;

namespace {

constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;
/* The most one command's body may fill, and a block of two such commands. */
constexpr std::size_t   largest = 64u;
constexpr std::size_t   framed = CommandHeader::encoded_size + largest;
constexpr std::uint32_t first_id = 0x10001u;
constexpr std::uint32_t refused = 0xC0000001u;

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);

/* A made transport over MockKmt, and a block of room for two whole commands. */
class Rig {
public:
    Rig() : connection_(kmt_), staging_{}, transport_(connection_, SubApi::opengl, 12u, reply_bytes, staging_,
                                                      sizeof staging_), block_{}
    {
        open();
    }
    /* One whose transport has memory for what outgrows the rooms. */
    explicit Rig(av::MockPlatform &memory)
        : connection_(kmt_), staging_{},
          transport_(connection_, memory, SubApi::opengl, 12u, reply_bytes, staging_, sizeof staging_), block_{}
    {
        open();
    }
    void open()
    {
        kmt_.add_adapter(av, 3200u);
        std::uint32_t adapter = 0u, count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
    }
    ~Rig() { transport_.teardown(); }

    MockKmt      &kmt() { return kmt_; }
    KmtTransport &inner() { return transport_; }
    std::uint8_t *block() { return block_; }

    /* A command of `bytes` of body through `batched`, sent async. */
    TransportResult send(BatchedTransport &batched, std::uint32_t id, std::size_t bytes)
    {
        WireWriter writer = batched.request_writer();
        writer.write_zeros(bytes);
        return batched.send_async(id, writer);
    }

    bool replies(std::uint32_t index)
    {
        CommandHeader header;
        return header.decode(kmt_.command_bytes(index), CommandHeader::encoded_size) == Status::ok &&
               header.generates_reply();
    }

private:
    MockKmt         kmt_;
    GuestConnection connection_;
    std::uint8_t    staging_[staging_bytes];
    KmtTransport    transport_;
    std::uint8_t    block_[2u * framed];
};

}  // namespace

TEST(batched_transport, async_commands_are_held_until_a_flush_sends_them_as_one_batch)
{ Rig rig;
  BatchedTransport batched(rig.inner(), rig.block(), 2u * framed, largest);
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK(rig.send(batched, first_id, 4u) == TransportResult::ok);
  CHECK(rig.send(batched, first_id + 1u, 12u) == TransportResult::ok);
  CHECK_EQ(rig.kmt().commands(), 0u);
  CHECK_EQ(batched.pending(), 2u * CommandHeader::encoded_size + 16u);
  CHECK(batched.flush() == TransportResult::ok);
  CHECK_EQ(batched.pending(), 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 1u);
  CHECK_EQ(rig.kmt().commands(), 2u);
  CHECK_EQ(rig.kmt().command_id(0u), first_id);
  CHECK_EQ(rig.kmt().command_size(0u), CommandHeader::encoded_size + 4u);
  CHECK_EQ(rig.kmt().command_id(1u), first_id + 1u);
  CHECK_EQ(rig.kmt().command_size(1u), CommandHeader::encoded_size + 12u);
  CHECK(!rig.replies(0u));
  CHECK(!rig.replies(1u));
  /* Nothing held: a flush sends nothing. */
  CHECK(batched.flush() == TransportResult::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 1u); }

TEST(batched_transport, a_command_that_wants_an_answer_ends_the_batch_and_is_answered)
{ Rig rig;
  BatchedTransport batched(rig.inner(), rig.block(), 2u * framed, largest);
  const std::uint8_t answer[] = {9u, 8u, 7u};
  rig.kmt().queue_answer(answer, sizeof answer);
  CHECK(rig.send(batched, first_id, 4u) == TransportResult::ok);
  WireWriter writer = batched.request_writer();
  writer.write_u32(5u);
  WireReader reply(nullptr, 0u);
  CHECK(batched.send_sync(first_id + 1u, writer, reply) == TransportResult::ok);
  CHECK_EQ(reply.remaining(), 3u);
  CHECK_EQ(reply.read_u8(), 9u);
  batched.end_call();
  CHECK_EQ(batched.pending(), 0u);
  CHECK_EQ(rig.kmt().commands(), 2u);
  CHECK(!rig.replies(0u));
  CHECK(rig.replies(1u));
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(batched_transport, a_writer_the_block_has_no_room_for_sends_what_is_held_first)
{ Rig rig;
  BatchedTransport batched(rig.inner(), rig.block(), 2u * framed, largest);
  CHECK(rig.send(batched, first_id, largest) == TransportResult::ok);
  CHECK(rig.send(batched, first_id + 1u, largest) == TransportResult::ok);
  CHECK_EQ(rig.kmt().commands(), 0u);
  /* The third could not fit: asking for its writer sends the two. */
  CHECK(rig.send(batched, first_id + 2u, 1u) == TransportResult::ok);
  CHECK_EQ(rig.kmt().commands(), 2u);
  CHECK_EQ(batched.pending(), CommandHeader::encoded_size + 1u);
  /* A command larger than it said commands are is not framed. */
  WireWriter writer = batched.request_writer();
  writer.write_zeros(largest + 1u);
  CHECK(batched.send_async(first_id + 3u, writer) == TransportResult::failed);
  CHECK_EQ(batched.pending(), CommandHeader::encoded_size + 1u); }

TEST(batched_transport, a_block_too_small_for_one_command_lends_no_room)
{ Rig rig;
  BatchedTransport none(rig.inner(), nullptr, 2u * framed, largest);
  WireWriter       writer = none.request_writer();
  writer.write_u8(1u);
  CHECK(writer.status() != Status::ok);
  CHECK(none.send_async(first_id, writer) == TransportResult::failed);
  BatchedTransport small(rig.inner(), rig.block(), framed - 1u, largest);
  WireWriter       cramped = small.request_writer();
  cramped.write_u8(1u);
  CHECK(small.send_async(first_id, cramped) == TransportResult::failed);
  CHECK_EQ(small.pending(), 0u); }

/* A batch the kernel would not take faults the transport under it, and nothing
 * more goes through this one. */
TEST(batched_transport, a_batch_that_could_not_be_sent_fails_everything_after_it)
{ Rig rig;
  BatchedTransport batched(rig.inner(), rig.block(), 2u * framed, largest);
  CHECK(rig.send(batched, first_id, 4u) == TransportResult::ok);
  rig.kmt().fail_at(MockKmt::Call::submit, 1u, refused);
  CHECK(batched.flush() == TransportResult::failed);
  CHECK(batched.flush() == TransportResult::failed);
  WireWriter writer = batched.request_writer();
  writer.write_u8(1u);
  CHECK(batched.send_async(first_id, writer) == TransportResult::failed);
  WireReader reply(nullptr, 0u);
  CHECK(batched.send_sync(first_id, writer, reply) == TransportResult::failed);
  CHECK_EQ(reply.remaining(), 0u);

  Rig second;
  BatchedTransport answered(second.inner(), second.block(), 2u * framed, largest);
  second.kmt().fail_at(MockKmt::Call::submit, 1u, refused);
  WireWriter asking = answered.request_writer();
  asking.write_u8(1u);
  CHECK(answered.send_sync(first_id, asking, reply) == TransportResult::failed);
  CHECK(second.send(answered, first_id, 1u) == TransportResult::failed);

  /* A full block the kernel will not take lends the next command nothing. */
  Rig full;
  BatchedTransport held(full.inner(), full.block(), 2u * framed, largest);
  CHECK(full.send(held, first_id, largest) == TransportResult::ok);
  CHECK(full.send(held, first_id + 1u, largest) == TransportResult::ok);
  full.kmt().fail_at(MockKmt::Call::submit, 1u, refused);
  WireWriter refused_room = held.request_writer();
  refused_room.write_u8(1u);
  CHECK(refused_room.status() != Status::ok);
  CHECK_EQ(full.kmt().commands(), 0u); }

TEST(batched_transport, the_arenas_are_the_transports_under_it)
{ Rig rig;
  BatchedTransport batched(rig.inner(), rig.block(), 2u * framed, largest);
  CHECK(&batched.decode_arena() == &rig.inner().decode_arena());
  CHECK(&batched.blobs() == &rig.inner().blobs()); }

/* Docs/protocol.md, "Long commands": a command longer than the room it was
 * lent spills, and is framed in pieces behind what the block holds - the
 * block going out each time it fills - with the command itself last. */
TEST(batched_transport, a_command_longer_than_its_room_follows_what_is_held_in_pieces)
{ av::MockPlatform memory;
  Rig              rig(memory);
  BatchedTransport batched(rig.inner(), rig.block(), 2u * framed, largest);
  CHECK(rig.send(batched, first_id, 4u) == TransportResult::ok);
  WireWriter writer = batched.request_writer();
  for (std::uint32_t index = 0u; index < 300u; ++index) writer.write_u8(static_cast<std::uint8_t>(index));
  CHECK(writer.spilled());
  CHECK(batched.send_async(first_id + 1u, writer) == TransportResult::ok);
  CHECK(batched.flush() == TransportResult::ok);
  /* What the last flush sent ends in the command, with its rest. */
  const std::uint32_t last = rig.kmt().commands() - 1u;
  CHECK_EQ(rig.kmt().command_id(last), first_id + 1u);
  CHECK(!rig.replies(last));
  std::uint32_t pieces = 0u;
  for (std::uint32_t index = 0u; index < last; ++index)
      if (rig.kmt().command_id(index) == CommandHeader::piece_id) pieces++;
  CHECK(pieces >= 1u);
  /* And the context goes on: an answered command after it. */
  const std::uint8_t answer[] = {6u};
  rig.kmt().queue_answer(answer, sizeof answer);
  WireWriter asking = batched.request_writer();
  for (std::uint32_t index = 0u; index < 200u; ++index) asking.write_u8(1u);
  WireReader reply(nullptr, 0u);
  CHECK(batched.send_sync(first_id + 2u, asking, reply) == TransportResult::ok);
  CHECK_EQ(reply.read_u8(), 6u);
  batched.end_call();
  CHECK(rig.replies(rig.kmt().commands() - 1u));
  CHECK_EQ(batched.pending(), 0u);
  /* A writer whose spill had no memory sends nothing at all. */
  memory.fail_allocation_at(1u);
  WireWriter refused_writer = batched.request_writer();
  refused_writer.write_zeros(largest + 1u);
  CHECK(batched.send_async(first_id + 3u, refused_writer) == TransportResult::failed);
  CHECK_EQ(batched.pending(), 0u); }
