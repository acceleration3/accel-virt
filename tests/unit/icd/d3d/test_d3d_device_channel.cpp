#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/shipped_batch.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/command_recording.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"
#include "proto/generated/d3d12/d3d12_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::GuestConnection;
using av::KmtTransport;
using av::LockTrace;
using av::MockKmt;
using av::PciIdentity;
using av::RecordingThreadLock;
using av::ShippedBatch;
using av::Status;
using av::SubApi;
using av::WireReader;
using av::d3d::D3dCommandStream;
using av::d3d::D3dDeviceChannel;
using namespace av::d3dproto;
using av::d3d12proto::kCmd_DrawInstanced12;
using av::d3d12proto::av_d3d12_DrawInstanced12;

namespace av {

struct KmtTransportProbe {
    static std::uint32_t ring(const KmtTransport &t) { return t.allocations_[0]; }
};

}

using av::KmtTransportProbe;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint8_t  ring_order = 20u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::size_t   block_bytes = D3dCommandStream::command_room + 4096u;
constexpr std::size_t   lent_room = 4096u;
constexpr std::uint32_t device_a = 3u;
constexpr std::uint32_t device_b = 5u;

class Fixture {
public:
    Fixture()
        : connection_(kmt_), transport_(connection_, SubApi::d3d, ring_order, reply_bytes, staging_, staging_bytes),
          stream_(transport_), lock_(trace_, 'P'), a_(stream_, lock_, device_a),
          b_(stream_, lock_, device_b), twelve_(stream_, lock_, 0u)
    {
        kmt_.add_adapter(av, 3200u);
        std::uint32_t adapters[1] = {};
        std::uint32_t count = 0u;
        static_cast<void>(kmt_.enumerate_adapters(adapters, 1u, count));
        CHECK_STATUS(connection_.open(adapters[0]), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        transport_.hold_in(block_, block_bytes);
    }
    ~Fixture() { transport_.teardown(); }

    MockKmt          &kmt() { return kmt_; }
    KmtTransport     &transport() { return transport_; }
    D3dCommandStream &stream() { return stream_; }
    RecordingThreadLock &lock() { return lock_; }
    LockTrace        &trace() { return trace_; }
    D3dDeviceChannel &a() { return a_; }
    D3dDeviceChannel &b() { return b_; }
    D3dDeviceChannel &twelve() { return twelve_; }

    std::uint32_t shipped(std::uint32_t *ids, std::uint32_t *selected, std::uint32_t capacity)
    {
        ShippedBatch batch(kmt_, KmtTransportProbe::ring(transport_));
        CHECK_STATUS(batch.read(), Status::ok);
        std::uint32_t index = 0u;
        for (; index < batch.commands() && index < capacity; index++) {
            ids[index] = batch.header(index).id();
            selected[index] = 0u;
            if (ids[index] != kCmd_SelectDevice11) continue;
            WireReader body(batch.body(index), sizeof(std::uint32_t));
            selected[index] = body.read_u32();
        }
        return index;
    }

private:
    MockKmt             kmt_;
    GuestConnection     connection_;
    std::uint8_t        staging_[staging_bytes] = {};
    KmtTransport        transport_;
    std::uint8_t        block_[block_bytes] = {};
    D3dCommandStream    stream_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    D3dDeviceChannel    a_;
    D3dDeviceChannel    b_;
    D3dDeviceChannel    twelve_;
};

}

TEST(d3d_device_channel, a_device_is_selected_only_when_another_spoke_last)
{ Fixture fixture;
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(av_d3d_Draw(fixture.a(), 6u, 0u));
  CHECK(av_d3d_Draw(fixture.b(), 9u, 0u));
  CHECK(av_d3d_Draw(fixture.a(), 12u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  std::uint32_t ids[8] = {}, selected[8] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 7u);
  const std::uint32_t want[7] = {kCmd_SelectDevice11, kCmd_Draw, kCmd_Draw, kCmd_SelectDevice11, kCmd_Draw,
                                 kCmd_SelectDevice11, kCmd_Draw};
  for (std::uint32_t index = 0u; index < 7u; index++) CHECK_EQ(ids[index], want[index]);
  CHECK_EQ(selected[0], device_a);
  CHECK_EQ(selected[3], device_b);
  CHECK_EQ(selected[5], device_a);
  CHECK(fixture.a().invariants_hold());
  CHECK_EQ(fixture.a().device(), device_a); }

TEST(d3d_device_channel, the_lock_is_held_for_each_call_and_no_longer)
{ Fixture fixture;
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(!fixture.lock().held());
  CHECK(av_d3d_Draw(fixture.b(), 3u, 0u));
  CHECK_STATUS(fixture.b().flush(), Status::ok);
  const std::uint64_t none = 0u;
  CHECK(fixture.b().make_room_for_blobs(&none, 1u));
  CHECK(std::strcmp(fixture.trace().text(), "PpPpPp") == 0);
  const std::uint64_t past_staging = fixture.transport().room().inline_write() + 1u;
  CHECK(!fixture.b().make_room_for_blobs(&past_staging, 1u));
  CHECK(!fixture.lock().held());
  CHECK(std::strcmp(fixture.trace().text(), "PpPpPpPp") == 0); }

TEST(d3d_device_channel, a_synchronous_call_keeps_the_lock_until_it_ends)
{ Fixture fixture;
  std::uint8_t *reply = fixture.kmt().reply_of(fixture.transport().context());
  CHECK(reply != nullptr);
  static_cast<void>(av::ReplyHeader(1u, 4u).encode(reply, av::ReplyHeader::encoded_size));
  const std::uint32_t removed = 0x887A0005u;
  std::memcpy(reply + av::ReplyHeader::encoded_size, &removed, sizeof removed);
  std::uint32_t answered = 0u;
  CHECK(av_d3d_Finish11(fixture.a(), &answered));
  CHECK_EQ(answered, removed);
  CHECK(!fixture.lock().held());
  CHECK(std::strcmp(fixture.trace().text(), "Pp") == 0); }

TEST(d3d_device_channel, room_held_commands_hold_comes_back_once_they_are_sent_and_run)
{ Fixture fixture;
  alignas(64) static std::uint8_t block[2u * lent_room];
  static std::uint8_t             upload[sizeof block];
  av::ClientBlobArena &arena = fixture.transport().blobs();
  arena.lend(block, sizeof block);
  const Box whole = {};
  CHECK(av_d3d_UpdateSubresource(fixture.a(), 1u, 0u, 0u, whole, 0u, 0u, 0u, upload, sizeof upload));
  CHECK_EQ(arena.room(), 0u);
  CHECK_EQ(fixture.kmt().commands(), 0u);
  const std::uint64_t past_lent = lent_room + 1u;
  CHECK(fixture.a().make_room_for_blobs(&past_lent, 1u));
  CHECK(arena.room() >= past_lent);
  std::uint32_t ids[4] = {}, selected[4] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 3u);
  CHECK_EQ(ids[1], kCmd_UpdateSubresource);
  CHECK_EQ(ids[2], av::CommandHeader::channel_stats_id);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, room_a_sent_batch_holds_comes_back_without_a_finish)
{ Fixture fixture;
  alignas(64) static std::uint8_t block[2u * lent_room];
  av::ClientBlobArena &arena = fixture.transport().blobs();
  arena.lend(block, sizeof block);
  CHECK(arena.take(arena.room()).taken());
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  const std::uint32_t sent = fixture.kmt().commands();
  const std::uint64_t past_lent = lent_room + 1u;
  CHECK(fixture.a().make_room_for_blobs(&past_lent, 1u));
  CHECK(arena.room() >= past_lent);
  CHECK_EQ(fixture.kmt().commands(), sent);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_command_given_up_before_it_was_sent_lets_the_lock_go)
{ Fixture fixture;
  const Box whole = {};
  CHECK(!av_d3d_UpdateSubresource(fixture.a(), 1u, 0u, 0u, whole, 0u, 0u, 0u, nullptr, 4u));
  CHECK(!fixture.lock().held());
  CHECK(av_d3d_Draw(fixture.b(), 3u, 0u));
  CHECK(std::strcmp(fixture.trace().text(), "PpPp") == 0); }

TEST(d3d_device_channel, closing_destroys_the_device_and_the_next_speaker_is_selected_again)
{ Fixture fixture;
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(fixture.a().close());
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  std::uint32_t ids[8] = {}, selected[8] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 5u);
  CHECK_EQ(ids[2], kCmd_DestroyDevice11);
  CHECK_EQ(ids[3], kCmd_SelectDevice11);
  CHECK_EQ(selected[3], device_a);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_failed_stream_fails_the_command_and_keeps_no_lock)
{ Fixture fixture;
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::invalid_state);
  CHECK(fixture.stream().failed());
  CHECK(!av_d3d_Draw(fixture.b(), 3u, 0u));
  CHECK(!fixture.b().close());
  CHECK(!fixture.lock().held()); }

namespace {
void answer_create(Fixture &fixture, std::uint32_t hresult)
{
    std::uint8_t *reply = fixture.kmt().reply_of(fixture.transport().context());
    CHECK(reply != nullptr);
    static_cast<void>(av::ReplyHeader(1u, 8u).encode(reply, av::ReplyHeader::encoded_size));
    const std::uint32_t words[2] = {hresult, 0xb100u};
    std::memcpy(reply + av::ReplyHeader::encoded_size, words, sizeof words);
}

}

TEST(d3d_device_channel, opening_selects_the_device_without_a_selection_sent_first)
{ Fixture fixture;
  answer_create(fixture, 0u);
  std::uint32_t hresult = 1u, level = 0u;
  CHECK(fixture.a().open(0xb100u, hresult, level));
  CHECK_EQ(hresult, 0u);
  CHECK_EQ(level, 0xb100u);
  std::uint32_t ids[4] = {}, selected[4] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 1u);
  CHECK_EQ(ids[0], kCmd_CreateDevice11);
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 1u);
  CHECK_EQ(ids[0], kCmd_Draw);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_refused_open_leaves_the_next_command_to_select)
{ Fixture fixture;
  answer_create(fixture, 0x80004005u);
  std::uint32_t hresult = 0u, level = 0u;
  CHECK(fixture.a().open(0xb100u, hresult, level));
  CHECK_EQ(hresult, 0x80004005u);
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  std::uint32_t ids[4] = {}, selected[4] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 2u);
  CHECK_EQ(ids[0], kCmd_SelectDevice11);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, an_open_that_cannot_be_sent_fails_and_keeps_no_lock)
{ Fixture fixture;
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  std::uint32_t hresult = 0u, level = 0u;
  CHECK(!fixture.a().open(0xb100u, hresult, level));
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_direct3d_12_speaker_selects_nothing_and_keeps_the_selection)
{ Fixture fixture;
  CHECK(av_d3d12_DrawInstanced12(fixture.twelve(), 1u, 3u, 1u, 0u, 0u));
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(av_d3d12_DrawInstanced12(fixture.twelve(), 1u, 3u, 1u, 0u, 0u));
  CHECK(av_d3d_Draw(fixture.a(), 6u, 0u));
  CHECK(!fixture.lock().held());
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  std::uint32_t ids[8] = {}, selected[8] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 5u);
  const std::uint32_t want[5] = {kCmd_DrawInstanced12, kCmd_SelectDevice11, kCmd_Draw, kCmd_DrawInstanced12,
                                 kCmd_Draw};
  for (std::uint32_t index = 0u; index < 5u; index++) CHECK_EQ(ids[index], want[index]);
  CHECK_EQ(selected[1], device_a);
  CHECK(fixture.twelve().invariants_hold());
  CHECK_EQ(fixture.twelve().device(), 0u); }

TEST(d3d_device_channel, a_recording_is_counted_and_taken_under_the_lock)
{ Fixture fixture;
  av::MockPlatform platform;
  av::d3d::CommandRecording recording(platform, block_bytes, block_bytes);
  CHECK_STATUS(recording.initialize(), Status::ok);
  fixture.stream().record(recording);
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  fixture.trace().clear();
  const std::size_t recorded = fixture.a().recorded_bytes();
  CHECK(recorded > av::CommandHeader::encoded_size);
  std::uint8_t out[256] = {};
  std::size_t  length = 0u;
  CHECK_STATUS(fixture.a().take_recorded(out, recorded - 1u, length), Status::invalid_parameter);
  CHECK_STATUS(fixture.a().take_recorded(out, sizeof out, length), Status::ok);
  CHECK_EQ(length, recorded);
  CHECK_EQ(fixture.a().recorded_bytes(), 0u);
  CHECK(!fixture.lock().held());
  CHECK(std::strcmp(fixture.trace().text(), "PpPpPpPp") == 0); }

TEST(d3d_device_channel, an_application_flush_is_the_streams_under_the_lock)
{ Fixture fixture;
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  fixture.trace().clear();
  CHECK_STATUS(fixture.a().application_flush(), Status::ok);
  CHECK(!fixture.lock().held());
  CHECK(std::strcmp(fixture.trace().text(), "Pp") == 0);
  std::uint32_t ids[8] = {}, selected[8] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 3u);
  CHECK_EQ(ids[1], kCmd_Draw);
  CHECK_EQ(ids[2], kCmd_Flush11);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u); }

namespace {
void answer_create9(Fixture &fixture, std::uint32_t hresult)
{
    std::uint8_t *reply = fixture.kmt().reply_of(fixture.transport().context());
    CHECK(reply != nullptr);
    static_cast<void>(av::ReplyHeader(1u, 4u).encode(reply, av::ReplyHeader::encoded_size));
    std::memcpy(reply + av::ReplyHeader::encoded_size, &hresult, sizeof hresult);
}

}

TEST(d3d_device_channel, a_direct3d_9_stream_selects_opens_and_closes_in_direct3d_9)
{ Fixture fixture;
  fixture.stream().speak_with(av::d3d9proto::kCmd_SelectDevice9, av::d3d9proto::kCmd_Flush9);
  answer_create9(fixture, 0u);
  std::uint32_t hresult = 1u;
  CHECK(fixture.a().open9(hresult));
  CHECK_EQ(hresult, 0u);
  std::uint32_t ids[6] = {}, selected[6] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 6u), 1u);
  CHECK_EQ(ids[0], av::d3d9proto::kCmd_CreateDevice9);
  CHECK(av::d3d9proto::av_d3d9_Flush9(fixture.a()));
  CHECK(av::d3d9proto::av_d3d9_Flush9(fixture.b()));
  CHECK(fixture.b().close9());
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  CHECK_EQ(fixture.shipped(ids, selected, 6u), 4u);
  CHECK_EQ(ids[0], av::d3d9proto::kCmd_Flush9);
  CHECK_EQ(ids[1], av::d3d9proto::kCmd_SelectDevice9);
  CHECK_EQ(ids[2], av::d3d9proto::kCmd_Flush9);
  CHECK_EQ(ids[3], av::d3d9proto::kCmd_Destroy9);
  CHECK(av::d3d9proto::av_d3d9_Flush9(fixture.b()));
  CHECK_STATUS(fixture.b().flush(), Status::ok);
  CHECK_EQ(fixture.shipped(ids, selected, 6u), 2u);
  CHECK_EQ(ids[0], av::d3d9proto::kCmd_SelectDevice9);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_direct3d_9_streams_application_flush_is_flush9)
{ Fixture fixture;
  fixture.stream().speak_with(av::d3d9proto::kCmd_SelectDevice9, av::d3d9proto::kCmd_Flush9);
  answer_create9(fixture, 0u);
  std::uint32_t hresult = 1u;
  CHECK(fixture.a().open9(hresult));
  std::uint32_t ids[4] = {}, selected[4] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 1u);
  CHECK(av::d3d9proto::av_d3d9_SetIndices9(fixture.a(), 7u));
  CHECK_STATUS(fixture.a().application_flush(), Status::ok);
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 2u);
  CHECK_EQ(ids[0], av::d3d9proto::kCmd_SetIndices9);
  CHECK_EQ(ids[1], av::d3d9proto::kCmd_Flush9);
  CHECK_STATUS(fixture.a().application_flush(), Status::ok);
  CHECK(av::d3d9proto::av_d3d9_SetIndices9(fixture.a(), 8u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 1u);
  CHECK_EQ(ids[0], av::d3d9proto::kCmd_SetIndices9);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, an_application_flush_after_the_selected_device_closed_selects_its_own)
{ Fixture fixture;
  fixture.stream().speak_with(av::d3d9proto::kCmd_SelectDevice9, av::d3d9proto::kCmd_Flush9);
  CHECK(av::d3d9proto::av_d3d9_SetIndices9(fixture.a(), 7u));
  CHECK(av::d3d9proto::av_d3d9_SetIndices9(fixture.b(), 8u));
  CHECK(fixture.b().close9());
  CHECK_STATUS(fixture.a().application_flush(), Status::ok);
  std::uint32_t ids[8] = {}, selected[8] = {};
  const std::uint32_t shipped = fixture.shipped(ids, selected, 8u);
  CHECK(shipped >= 2u);
  CHECK_EQ(ids[shipped - 2u], av::d3d9proto::kCmd_SelectDevice9);
  CHECK_EQ(ids[shipped - 1u], av::d3d9proto::kCmd_Flush9);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_direct3d_9_open_that_cannot_be_sent_fails_and_keeps_no_lock)
{ Fixture fixture;
  fixture.stream().speak_with(av::d3d9proto::kCmd_SelectDevice9, av::d3d9proto::kCmd_Flush9);
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  std::uint32_t hresult = 0u;
  CHECK(!fixture.a().open9(hresult));
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_refused_direct3d_9_open_leaves_the_next_command_to_select)
{ Fixture fixture;
  fixture.stream().speak_with(av::d3d9proto::kCmd_SelectDevice9, av::d3d9proto::kCmd_Flush9);
  answer_create9(fixture, 0x8876086Cu);
  std::uint32_t hresult = 0u;
  CHECK(fixture.a().open9(hresult));
  CHECK_EQ(hresult, 0x8876086Cu);
  CHECK(av::d3d9proto::av_d3d9_Flush9(fixture.a()));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  std::uint32_t ids[4] = {}, selected[4] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 2u);
  CHECK_EQ(ids[0], av::d3d9proto::kCmd_SelectDevice9);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_failed_streams_application_flush_fails_and_an_idle_call_ends_quietly)
{ Fixture fixture;
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::invalid_state);
  CHECK_STATUS(fixture.b().application_flush(), Status::invalid_state);
  fixture.a().end_call();
  CHECK(!fixture.lock().held()); }

namespace {
std::size_t recorded_draws(std::uint8_t *out, std::size_t capacity, std::uint32_t draws)
{
    Fixture         recorder;
    av::MockPlatform platform;
    av::d3d::CommandRecording recording(platform, block_bytes, block_bytes);
    CHECK_STATUS(recording.initialize(), Status::ok);
    recorder.stream().record(recording);
    for (std::uint32_t draw = 0u; draw < draws; draw++) CHECK(av_d3d_Draw(recorder.a(), 3u, 0u));
    std::size_t length = 0u;
    CHECK_STATUS(recorder.a().take_recorded(out, capacity, length), Status::ok);
    return length;
}

}

TEST(d3d_device_channel, a_command_list_runs_between_cleared_states)
{ std::uint8_t list[256] = {};
  const std::size_t length = recorded_draws(list, sizeof list, 1u);
  CHECK(length > 0u);
  Fixture fixture;
  CHECK_STATUS(fixture.a().execute(list, length), Status::ok);
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  std::uint32_t ids[8] = {}, selected[8] = {};
  const std::uint32_t first[5] = {kCmd_SelectDevice11, kCmd_ClearState11, kCmd_SelectDevice11, kCmd_Draw,
                                  kCmd_ClearState11};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 5u);
  for (std::uint32_t at = 0u; at < 5u; at++) CHECK_EQ(ids[at], first[at]);
  CHECK_STATUS(fixture.a().execute(list, length), Status::ok);
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  const std::uint32_t again[3] = {kCmd_SelectDevice11, kCmd_Draw, kCmd_ClearState11};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 3u);
  for (std::uint32_t at = 0u; at < 3u; at++) CHECK_EQ(ids[at], again[at]);
  CHECK(av_d3d_Draw(fixture.a(), 6u, 0u));
  CHECK_STATUS(fixture.a().execute(list, length), Status::ok);
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  const std::uint32_t after_draw[5] = {kCmd_Draw, kCmd_ClearState11, kCmd_SelectDevice11, kCmd_Draw,
                                       kCmd_ClearState11};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 5u);
  for (std::uint32_t at = 0u; at < 5u; at++) CHECK_EQ(ids[at], after_draw[at]);
  const std::uint8_t junk[4] = {1u, 2u, 3u, 4u};
  CHECK_STATUS(fixture.a().execute(junk, sizeof junk), Status::malformed);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_failed_stream_runs_no_command_list)
{ std::uint8_t list[256] = {};
  const std::size_t length = recorded_draws(list, sizeof list, 1u);
  Fixture fixture;
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::invalid_state);
  CHECK_STATUS(fixture.b().execute(list, length), Status::invalid_state);
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, a_flush_the_kernel_refuses_fails_the_command_list)
{ static std::uint8_t list[block_bytes] = {};
  constexpr std::size_t draw = av::CommandHeader::encoded_size + 8u;
  const std::size_t     length = recorded_draws(list, sizeof list, 300u);
  CHECK(length > 4096u);
  { Fixture fixture;
    while (fixture.stream().recorded_bytes() + length <= block_bytes) CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
    fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
    CHECK_STATUS(fixture.a().execute(list, length), Status::invalid_state);
    CHECK(!fixture.lock().held()); }
  const std::size_t nearly_whole = recorded_draws(list, sizeof list, static_cast<std::uint32_t>((block_bytes - 512u) / draw));
  CHECK(nearly_whole + draw + staging_bytes > block_bytes);
  { Fixture fixture;
    fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
    CHECK_STATUS(fixture.a().execute(list, nearly_whole), Status::invalid_state);
    CHECK(fixture.stream().failed());
    CHECK(!fixture.lock().held()); } }

TEST(d3d_device_channel, a_refused_flush_for_the_clear_before_a_list_fails_it)
{ std::uint8_t list[256] = {};
  const std::size_t length = recorded_draws(list, sizeof list, 1u);
  std::uint32_t fitting = 0u;
  { Fixture probe;
    for (std::size_t before = 0u; probe.a().recorded_bytes() >= before; fitting++) {
        before = probe.a().recorded_bytes();
        CHECK(av_d3d_Draw(probe.a(), 3u, 0u));
    }
    fitting--; }
  CHECK(fitting > 1u);
  Fixture fixture;
  for (std::uint32_t draw = 0u; draw < fitting; draw++) CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK_STATUS(fixture.a().execute(list, length), Status::invalid_state);
  CHECK(fixture.stream().failed());
  CHECK(!fixture.lock().held()); }

TEST(d3d_device_channel, the_publish_barrier_runs_what_is_held_under_the_lock)
{ Fixture fixture;
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  fixture.trace().clear();
  fixture.kmt().queue_answer(nullptr, 0u);
  CHECK_STATUS(fixture.a().run_held(), Status::ok);
  CHECK(std::strcmp(fixture.trace().text(), "Pp") == 0);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 1u);
  std::uint32_t ids[4] = {}, selected[4] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 3u);
  CHECK_EQ(ids[1], kCmd_Draw);
  CHECK_EQ(ids[2], av::CommandHeader::nop_id);
  CHECK(!fixture.stream().failed());
  fixture.kmt().withhold_answer_at(1u);
  CHECK_STATUS(fixture.a().run_held(), Status::invalid_state);
  CHECK(fixture.stream().failed());
  CHECK_STATUS(fixture.a().run_held(), Status::invalid_state);
  CHECK(!fixture.lock().held());
  Fixture recording;
  av::MockPlatform          platform;
  av::d3d::CommandRecording recorded(platform, block_bytes, block_bytes);
  CHECK_STATUS(recorded.initialize(), Status::ok);
  recording.stream().record(recorded);
  CHECK_STATUS(recording.a().run_held(), Status::invalid_state);
  CHECK_EQ(recording.kmt().commands(), 0u);
  CHECK(!recording.stream().failed()); }
