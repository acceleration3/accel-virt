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
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/command_recording.hpp"
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
using av::MockPlatform;
using av::PciIdentity;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireReader;
using av::d3d::CommandRecording;
using av::d3d::D3dCommandStream;
using namespace av::d3dproto;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
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
        kmt_.add_adapter(av, 3200u);
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

/* A deferred context's recording block, first the stream's size and growing
 * to `most`. */
class Recorded {
public:
    explicit Recorded(std::size_t most) : recording_(platform_, block_bytes, most)
    {
        CHECK_STATUS(recording_.initialize(), Status::ok);
    }

    CommandRecording &recording() { return recording_; }
    MockPlatform     &platform() { return platform_; }

private:
    MockPlatform     platform_;
    CommandRecording recording_;
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
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
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

/* An application's flush sends its batch even to a host still behind: what
 * another device reads next is released at that flush, and the batches only
 * run in order once both are published. */
TEST(d3d_command_stream, an_application_flush_sends_its_batch_while_the_host_is_behind)
{ Fixture fixture;
  for (std::uint32_t batch = 0u; batch < 2u; batch++) {
      CHECK(av_d3d_Draw(fixture.stream(), batch, 0u));
      CHECK_STATUS(fixture.stream().flush(), Status::ok);
  }
  const std::uint32_t submits = fixture.submits();
  CHECK(av_d3d_Draw(fixture.stream(), 9u, 0u));
  CHECK_STATUS(fixture.stream().flush_hint(), Status::ok);
  CHECK_EQ(fixture.submits(), submits);
  CHECK_STATUS(fixture.stream().application_flush(), Status::ok);
  CHECK_EQ(fixture.submits(), submits + 1u);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  CHECK(fixture.stream().invariants_hold()); }

/* The host holding more than the newest batch is not ready: a hint leaves the
 * batch growing, and a real flush sends it. */
TEST(d3d_command_stream, a_flush_hint_waits_while_the_host_is_behind)
{ Fixture fixture;
  for (std::uint32_t batch = 0u; batch < 2u; batch++) {
      CHECK(av_d3d_Draw(fixture.stream(), batch, 0u));
      CHECK_STATUS(fixture.stream().flush(), Status::ok);
  }
  const std::uint32_t submits = fixture.submits();
  CHECK(av_d3d_Draw(fixture.stream(), 9u, 0u));
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

/* Commands past stream_bytes go to an idle host at once, so its replay runs
 * beside the recording; a host still behind gets them only when the batch
 * must go. */
TEST(d3d_command_stream, an_idle_host_is_sent_what_is_recorded_once_it_passes_the_stream_size)
{ Fixture fixture;
  const std::uint32_t submits = fixture.submits();
  std::size_t last_pending = 0u;
  for (std::uint32_t draw = 0u; draw < 1000u && fixture.submits() == submits; draw++) {
      last_pending = fixture.stream().recorded_bytes();
      CHECK(av_d3d_Draw(fixture.stream(), draw, 0u));
  }
  CHECK_EQ(fixture.submits(), submits + 1u);
  CHECK(last_pending < D3dCommandStream::stream_bytes);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);

  Fixture behind;
  for (std::uint32_t batch = 0u; batch < 2u; batch++) {
      CHECK(av_d3d_Draw(behind.stream(), batch, 0u));
      CHECK_STATUS(behind.stream().flush(), Status::ok);
  }
  const std::uint32_t queued = behind.submits();
  for (std::uint32_t draw = 0u; draw < 200u; draw++) CHECK(av_d3d_Draw(behind.stream(), draw, 0u));
  CHECK(behind.stream().recorded_bytes() >= D3dCommandStream::stream_bytes);
  CHECK_EQ(behind.submits(), queued);
  CHECK(fixture.stream().invariants_hold()); }

/* A deferred context's stream records: nothing is sent, nothing may be
 * answered, data goes inline, and the recording is handed over whole. */
TEST(d3d_command_stream, a_recording_is_kept_and_taken_out_whole)
{ Fixture fixture;
  Recorded block(block_bytes);
  fixture.stream().record(block.recording());
  CHECK(!fixture.stream().blobs().ready());
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK(av_d3d_Draw(fixture.stream(), 6u, 0u));
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.submits(), 0u);
  CHECK_EQ(fixture.kmt().commands(), 0u);
  std::uint32_t removed = 0u;
  CHECK(!av_d3d_Finish11(fixture.stream(), &removed));
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

/* A recording is never sent to make room: one that cannot grow fills its
 * block to the last byte, a command past that is refused rather than written
 * beyond it, and the take reports the recording incomplete and starts the
 * next one afresh. */
TEST(d3d_command_stream, a_recording_that_outgrows_the_block_is_refused_whole)
{ Fixture fixture;
  Recorded recorded(block_bytes);
  fixture.stream().record(recorded.recording());
  constexpr std::size_t draw = CommandHeader::encoded_size + 8u;
  static_assert(block_bytes % draw == 0u, "the block holds a whole number of draws");
  for (std::size_t index = 0u; index < block_bytes / draw; index++)
      CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK_EQ(fixture.stream().recorded_bytes(), block_bytes);
  CHECK(!av_d3d_Draw(fixture.stream(), 3u, 0u));
  /* A command with no body is lent nothing too, and is refused as well. */
  CHECK(!av_d3d_ClearState11(fixture.stream()));
  CHECK_EQ(fixture.stream().recorded_bytes(), block_bytes);
  CHECK(fixture.stream().invariants_hold());
  CHECK(!fixture.stream().failed());
  static std::uint8_t out[block_bytes];
  std::size_t length = 99u;
  CHECK_STATUS(fixture.stream().take_recorded(out, sizeof out, length), Status::no_space);
  CHECK_EQ(length, 0u);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  CHECK_EQ(fixture.submits(), 0u);

  /* The next recording is whole again. */
  CHECK(av_d3d_Draw(fixture.stream(), 6u, 0u));
  CHECK_STATUS(fixture.stream().take_recorded(out, sizeof out, length), Status::ok);
  CHECK_EQ(length, draw); }

/* A recording that may grow does, carrying what it holds over, and the take
 * hands all of it out. */
TEST(d3d_command_stream, a_recording_grows_to_hold_what_is_recorded)
{ Fixture fixture;
  Recorded recorded(4u * block_bytes);
  fixture.stream().record(recorded.recording());
  constexpr std::size_t draw = CommandHeader::encoded_size + 8u;
  constexpr std::size_t draws = 2u * block_bytes / draw;
  for (std::size_t index = 0u; index < draws; index++)
      CHECK(av_d3d_Draw(fixture.stream(), static_cast<std::uint32_t>(index), 0u));
  CHECK_EQ(fixture.stream().recorded_bytes(), draws * draw);
  CHECK(recorded.recording().capacity() > block_bytes);
  CHECK(fixture.stream().invariants_hold());
  static std::uint8_t out[2u * block_bytes];
  std::size_t length = 0u;
  CHECK_STATUS(fixture.stream().take_recorded(out, sizeof out, length), Status::ok);
  CHECK_EQ(length, draws * draw);
  /* The last draw, recorded after the block grew, is the last one made. */
  WireReader last(out + length - 8u, 8u);
  std::uint32_t vertices = 0u;
  CHECK(last.read_pod(vertices));
  CHECK_EQ(vertices, static_cast<std::uint32_t>(draws - 1u));
  /* The first, from before, came over intact. */
  WireReader first(out + CommandHeader::encoded_size, 8u);
  CHECK(first.read_pod(vertices));
  CHECK_EQ(vertices, 0u);
  CHECK_EQ(fixture.submits(), 0u);
  CHECK_EQ(recorded.platform().live_allocations(), 1u); }

/* A recording refused a larger block is lent what is left of its own, and
 * asks again at its next command. */
TEST(d3d_command_stream, a_recording_refused_a_larger_block_keeps_its_own_until_the_next_command)
{ Fixture fixture;
  Recorded recorded(4u * block_bytes);
  fixture.stream().record(recorded.recording());
  constexpr std::size_t draw = CommandHeader::encoded_size + 8u;
  /* Every draw is lent a whole command's room until less than that is left. */
  constexpr std::size_t roomy = (block_bytes - CommandHeader::encoded_size - D3dCommandStream::command_room) / draw + 1u;
  for (std::size_t index = 0u; index < roomy; index++) CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK_EQ(recorded.recording().capacity(), block_bytes);
  recorded.platform().fail_allocation_at(1u);
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK_EQ(recorded.recording().capacity(), block_bytes);
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK_EQ(recorded.recording().capacity(), 2u * block_bytes);
  static std::uint8_t out[block_bytes];
  std::size_t length = 0u;
  CHECK_STATUS(fixture.stream().take_recorded(out, sizeof out, length), Status::ok);
  CHECK_EQ(length, (roomy + 2u) * draw); }

/* A recording has no arena: it is told it has room for what one command
 * carries inline, a command whose data does not fit is lost rather than
 * recorded short, and the take reports the recording incomplete. */
TEST(d3d_command_stream, a_recorded_command_whose_data_does_not_fit_leaves_the_recording_incomplete)
{ Fixture fixture;
  Recorded recorded(block_bytes);
  fixture.stream().record(recorded.recording());
  CHECK(fixture.stream().make_room_for(D3dCommandStream::recorded_data));
  CHECK(!fixture.stream().make_room_for(D3dCommandStream::recorded_data + 1u));
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  static std::uint8_t data[D3dCommandStream::command_room];
  const Box box{};
  CHECK(!av_d3d_UpdateSubresource(fixture.stream(), 1u, 0u, 0u, box, 0u, 0u, 0u, data, sizeof data));
  CHECK_EQ(fixture.stream().recorded_bytes(), CommandHeader::encoded_size + 8u);
  static std::uint8_t out[block_bytes];
  std::size_t length = 99u;
  CHECK_STATUS(fixture.stream().take_recorded(out, sizeof out, length), Status::no_space);
  CHECK_EQ(length, 0u);

  /* As much as one recorded command carries goes inline whole. */
  CHECK(av_d3d_UpdateSubresource(fixture.stream(), 1u, 0u, 0u, box, 0u, 0u, 0u, data,
                                  D3dCommandStream::recorded_data));
  CHECK(fixture.stream().recorded_bytes() > D3dCommandStream::recorded_data);
  CHECK_STATUS(fixture.stream().take_recorded(out, sizeof out, length), Status::ok);
  CHECK_EQ(length, fixture.stream().recorded_total() - (CommandHeader::encoded_size + 8u));
  CHECK_EQ(fixture.submits(), 0u); }

/* A recording run on the immediate stream joins what is pending; one larger
 * than the block goes a block's worth of whole commands at a time, what is
 * pending sent first to make room. */
TEST(d3d_command_stream, recorded_commands_join_the_stream_a_block_of_whole_commands_at_a_time)
{ Fixture fixture;
  CHECK_STATUS(fixture.stream().append_recorded(nullptr, 0u), Status::ok);
  CHECK_STATUS(fixture.stream().append_recorded(nullptr, 16u), Status::invalid_parameter);
  static std::uint8_t list[2u * block_bytes];
  const std::size_t small = frame_draws(list, 2u);
  const std::uint64_t before = fixture.stream().recorded_total();
  CHECK_STATUS(fixture.stream().append_recorded(list, small), Status::ok);
  CHECK_EQ(fixture.stream().recorded_bytes(), small);
  CHECK_EQ(fixture.stream().recorded_total(), before + small);
  CHECK_EQ(fixture.submits(), 0u);
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(fixture.id(1u), kCmd_Draw);

  /* Larger than the block: the pending draw goes first, then a full block of
   * the list, and the rest waits in the block. */
  constexpr std::size_t draw = CommandHeader::encoded_size + 8u;
  CHECK(av_d3d_Draw(fixture.stream(), 1u, 0u));
  const std::size_t large = frame_draws(list, block_bytes / draw + 4u);
  CHECK(large > block_bytes);
  const std::uint32_t submits = fixture.submits();
  CHECK_STATUS(fixture.stream().append_recorded(list, large), Status::ok);
  CHECK_EQ(fixture.submits(), submits + 2u);
  CHECK_EQ(fixture.stream().recorded_bytes(), 4u * draw);
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.submits(), submits + 3u);
  CHECK_EQ(fixture.stream().recorded_total(), before + small + draw + large);
  CHECK(!fixture.stream().failed()); }

/* Bytes that are not whole commands are refused where they stop being ones,
 * after the whole ones before them. */
TEST(d3d_command_stream, recorded_bytes_that_are_not_whole_commands_are_refused)
{ Fixture fixture;
  std::uint8_t list[64] = {};
  const std::size_t two = frame_draws(list, 2u);
  CHECK_STATUS(fixture.stream().append_recorded(list, two - 1u), Status::malformed);
  CHECK_EQ(fixture.stream().recorded_bytes(), CommandHeader::encoded_size + 8u);
  /* A header claiming less than itself. */
  CHECK_STATUS(CommandHeader(kCmd_Draw, 4u, 0u).encode(list, CommandHeader::encoded_size), Status::ok);
  CHECK_STATUS(fixture.stream().append_recorded(list, two), Status::malformed);
  CHECK(!fixture.stream().failed()); }

/* A deferred context running a command list records it, growing to hold it;
 * one that cannot grow leaves the recording incomplete. */
TEST(d3d_command_stream, a_recording_appends_a_list_by_growing)
{ Fixture fixture;
  Recorded recorded(4u * block_bytes);
  fixture.stream().record(recorded.recording());
  static std::uint8_t list[2u * block_bytes];
  constexpr std::size_t draw = CommandHeader::encoded_size + 8u;
  const std::size_t large = frame_draws(list, block_bytes / draw + 4u);
  CHECK_STATUS(fixture.stream().append_recorded(list, large), Status::ok);
  CHECK_EQ(fixture.stream().recorded_bytes(), large);
  CHECK_EQ(fixture.submits(), 0u);

  Fixture other;
  Recorded fixed(block_bytes);
  other.stream().record(fixed.recording());
  CHECK_STATUS(other.stream().append_recorded(list, large), Status::no_space);
  static std::uint8_t out[block_bytes];
  std::size_t length = 0u;
  CHECK_STATUS(other.stream().take_recorded(out, sizeof out, length), Status::no_space); }

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
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
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
