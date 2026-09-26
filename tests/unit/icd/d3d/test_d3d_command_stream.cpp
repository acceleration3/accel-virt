/* One process's Direct3D commands, batched until they must go: an
 * application's Flush sends only when the host is ready for more, a deferred
 * context records instead of sending and hands its recording over, and a
 * recording run later is appended to the stream or sent whole. What the host
 * received is read off MockKmt's ring, which journals every command of a
 * batch. */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

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
using av::d3d::D3dCommandStream;
using namespace av::d3dproto;

namespace {

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint8_t  ring_order = 20u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::size_t   block_bytes = D3dCommandStream::command_room + 4096u;
constexpr std::uint32_t refused = 0xC0000001u;

class Fixture {
public:
    Fixture()
        : connection_(kmt_), transport_(connection_, SubApi::d3d, ring_order, reply_bytes, staging_, staging_bytes),
          stream_(transport_, block_, block_bytes)
    {
        kmt_.add_adapter(rvk, 3200u);
        std::uint32_t adapters[1] = {};
        std::uint32_t count = 0u;
        static_cast<void>(kmt_.enumerate_adapters(adapters, 1u, count));
        CHECK_STATUS(connection_.open(adapters[0]), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
    }
    ~Fixture() { transport_.teardown(); }

    MockKmt          &kmt() { return kmt_; }
    D3dCommandStream &stream() { return stream_; }
    std::uint32_t     submits() { return kmt_.calls(MockKmt::Call::submit); }
    std::uint32_t     id(std::uint32_t index) { return kmt_.command_id(index); }

private:
    MockKmt          kmt_;
    GuestConnection  connection_;
    std::uint8_t     staging_[staging_bytes] = {};
    KmtTransport     transport_;
    std::uint8_t     block_[block_bytes] = {};
    D3dCommandStream stream_;
};

/* `count` framed Draw commands, each a header and eight bytes of body. */
std::size_t frame_draws(std::uint8_t *bytes, std::size_t count)
{
    constexpr std::uint32_t size = CommandHeader::encoded_size + 8u;
    for (std::size_t index = 0u; index < count; index++)
        CHECK_STATUS(CommandHeader(kCmd_Draw, size, 0u).encode(bytes + size * index, CommandHeader::encoded_size),
                     Status::ok);
    return size * count;
}

}  // namespace

/* An application's Flush asks the host to flush its own queue too - once per
 * run of recorded commands - and sends the batch while the host is ready. */
TEST(d3d_command_stream, an_application_flush_asks_the_host_once_and_sends_the_batch)
{ Fixture fixture;
  CHECK(rvk_d3d_Draw(fixture.stream(), 3u, 0u));
  const std::uint64_t drawn = fixture.stream().recorded_total();
  CHECK(drawn != 0u);
  CHECK_STATUS(fixture.stream().application_flush(), Status::ok);
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(fixture.id(0u), kCmd_Draw);
  CHECK_EQ(fixture.id(1u), kCmd_Flush11);
  CHECK(fixture.stream().recorded_total() > drawn);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  /* Nothing recorded since: no second Flush11, and nothing to send. */
  const std::uint32_t submits = fixture.submits();
  CHECK_STATUS(fixture.stream().application_flush(), Status::ok);
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(fixture.submits(), submits);
  CHECK(fixture.stream().invariants_hold()); }

/* The host holding more than the newest batch is not ready: a hint leaves the
 * batch growing, and a real flush sends it. */
TEST(d3d_command_stream, a_flush_hint_waits_while_the_host_is_behind)
{ Fixture fixture;
  for (std::uint32_t batch = 0u; batch < 2u; batch++) {
      CHECK(rvk_d3d_Draw(fixture.stream(), batch, 0u));
      CHECK_STATUS(fixture.stream().flush(), Status::ok);
  }
  const std::uint32_t submits = fixture.submits();
  CHECK(rvk_d3d_Draw(fixture.stream(), 9u, 0u));
  const std::size_t pending = fixture.stream().recorded_bytes();
  CHECK(pending != 0u);
  CHECK_STATUS(fixture.stream().flush_hint(), Status::ok);
  CHECK_EQ(fixture.submits(), submits);
  CHECK_EQ(fixture.stream().recorded_bytes(), pending);
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.submits(), submits + 1u);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  /* Nothing pending: a hint is nothing to send. */
  CHECK_STATUS(fixture.stream().flush_hint(), Status::ok);
  CHECK_EQ(fixture.submits(), submits + 1u); }

/* A deferred context's stream records: nothing is sent, nothing may be
 * answered, data goes inline, and the recording is handed over whole. */
TEST(d3d_command_stream, a_recording_is_kept_and_taken_out_whole)
{ Fixture fixture;
  fixture.stream().record();
  CHECK(!fixture.stream().blobs().ready());
  CHECK(rvk_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK(rvk_d3d_Draw(fixture.stream(), 6u, 0u));
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.submits(), 0u);
  CHECK_EQ(fixture.kmt().commands(), 0u);
  std::uint32_t removed = 0u;
  CHECK(!rvk_d3d_Finish11(fixture.stream(), &removed));
  const std::size_t recorded = fixture.stream().recorded_bytes();
  CHECK_EQ(recorded, 2u * (CommandHeader::encoded_size + 8u));
  std::uint8_t out[64] = {};
  std::size_t  length = 99u;
  /* Too small to hold it: refused, and nothing is forgotten. */
  CHECK_STATUS(fixture.stream().take_recorded(out, recorded - 1u, length), Status::invalid_parameter);
  CHECK_EQ(length, 0u);
  CHECK_STATUS(fixture.stream().take_recorded(nullptr, sizeof out, length), Status::invalid_parameter);
  CHECK_EQ(fixture.stream().recorded_bytes(), recorded);
  CHECK_STATUS(fixture.stream().take_recorded(out, sizeof out, length), Status::ok);
  CHECK_EQ(length, recorded);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  CommandHeader first;
  CHECK_STATUS(first.decode(out, CommandHeader::encoded_size), Status::ok);
  CHECK_EQ(first.id(), kCmd_Draw);
  /* Nothing left: a take of nothing needs nowhere to put it. */
  CHECK_STATUS(fixture.stream().take_recorded(nullptr, 0u, length), Status::ok);
  CHECK_EQ(length, 0u); }

/* A recording run on the immediate stream joins what is pending; one larger
 * than the block goes after it as a batch of its own. */
TEST(d3d_command_stream, recorded_commands_join_the_stream_or_go_as_their_own_batch)
{ Fixture fixture;
  CHECK_STATUS(fixture.stream().append_recorded(nullptr, 0u), Status::ok);
  CHECK_STATUS(fixture.stream().append_recorded(nullptr, 16u), Status::invalid_parameter);
  static std::uint8_t list[block_bytes + 64u];
  const std::size_t small = frame_draws(list, 2u);
  const std::uint64_t before = fixture.stream().recorded_total();
  CHECK_STATUS(fixture.stream().append_recorded(list, small), Status::ok);
  CHECK_EQ(fixture.stream().recorded_bytes(), small);
  CHECK_EQ(fixture.stream().recorded_total(), before + small);
  CHECK_EQ(fixture.submits(), 0u);
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(fixture.id(1u), kCmd_Draw);

  /* Larger than the block: what is pending goes first, then the list whole. */
  CHECK(rvk_d3d_Draw(fixture.stream(), 1u, 0u));
  const std::size_t large = frame_draws(list, (block_bytes + 64u) / (CommandHeader::encoded_size + 8u));
  CHECK(large > block_bytes);
  const std::uint32_t submits = fixture.submits();
  CHECK_STATUS(fixture.stream().append_recorded(list, large), Status::ok);
  CHECK_EQ(fixture.submits(), submits + 2u);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  CHECK(!fixture.stream().failed()); }

/* The host is told which device speaks only when the speaker changes; a
 * Direct3D 12 speaker selects nothing. */
TEST(d3d_command_stream, a_device_is_selected_only_when_the_speaker_changes)
{ Fixture fixture;
  CHECK(fixture.stream().speak_as(0u));
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  CHECK(fixture.stream().speak_as(3u));
  const std::size_t selected = fixture.stream().recorded_bytes();
  CHECK(selected != 0u);
  CHECK(fixture.stream().speak_as(3u));
  CHECK(fixture.stream().speak_as(0u));
  CHECK_EQ(fixture.stream().recorded_bytes(), selected);
  /* The host selected 5 itself: nothing to send for it. */
  fixture.stream().selected(5u);
  CHECK(fixture.stream().speak_as(5u));
  CHECK_EQ(fixture.stream().recorded_bytes(), selected);
  /* Another device's end leaves the selection; its own clears it. */
  fixture.stream().forget(3u);
  CHECK(fixture.stream().speak_as(5u));
  CHECK_EQ(fixture.stream().recorded_bytes(), selected);
  fixture.stream().forget(5u);
  CHECK(fixture.stream().speak_as(5u));
  CHECK(fixture.stream().recorded_bytes() > selected);
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.id(0u), kCmd_SelectDevice11);
  CHECK_EQ(fixture.id(1u), kCmd_SelectDevice11);
  CHECK_EQ(fixture.kmt().commands(), 2u); }

/* A batch the kernel refused loses the device: every later call fails. */
TEST(d3d_command_stream, a_refused_batch_fails_everything_after_it)
{ Fixture fixture;
  CHECK(rvk_d3d_Draw(fixture.stream(), 3u, 0u));
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, refused);
  CHECK_STATUS(fixture.stream().flush(), Status::invalid_state);
  CHECK(fixture.stream().failed());
  CHECK_STATUS(fixture.stream().flush(), Status::invalid_state);
  CHECK_STATUS(fixture.stream().flush_hint(), Status::invalid_state);
  std::size_t length = 0u;
  CHECK_STATUS(fixture.stream().take_recorded(nullptr, 0u, length), Status::invalid_state);
  std::uint8_t list[32] = {};
  CHECK_STATUS(fixture.stream().append_recorded(list, frame_draws(list, 2u)), Status::invalid_state);
  CHECK(!fixture.stream().speak_as(0u));
  CHECK(!fixture.stream().speak_as(7u));
  CHECK_STATUS(fixture.stream().application_flush(), Status::invalid_state);
  CHECK(fixture.stream().invariants_hold()); }
