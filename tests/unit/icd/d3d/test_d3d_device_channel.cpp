/* A process's devices on one command stream: each command is recorded under
 * the process's lock, a device that was not the last to speak is selected
 * before its command and one that was is not, and the lock is let go when an
 * asynchronous command is recorded, when a synchronous call ends, and when a
 * command is given up before it was sent - a lock kept there would hang the
 * process's next call on any device. Closing destroys the device on the host
 * and makes the stream select whoever speaks next. The batch is read back off
 * MockKmt's ring as the device reads it. */
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
using av::d3d12proto::rvk_d3d12_DrawInstanced12;

namespace av {

struct KmtTransportProbe {
    static std::uint32_t ring(const KmtTransport &t) { return t.allocations_[0]; }
};

}  // namespace av

using av::KmtTransportProbe;

namespace {

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint8_t  ring_order = 16u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::size_t   block_bytes = D3dCommandStream::command_room + 4096u;
constexpr std::uint32_t device_a = 3u;
constexpr std::uint32_t device_b = 5u;

class Fixture {
public:
    Fixture()
        : connection_(kmt_), transport_(connection_, SubApi::d3d, ring_order, reply_bytes, staging_, staging_bytes),
          stream_(transport_, block_, block_bytes), lock_(trace_, 'P'), a_(stream_, lock_, device_a),
          b_(stream_, lock_, device_b), twelve_(stream_, lock_, 0u)
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
    KmtTransport     &transport() { return transport_; }
    D3dCommandStream &stream() { return stream_; }
    RecordingThreadLock &lock() { return lock_; }
    LockTrace        &trace() { return trace_; }
    D3dDeviceChannel &a() { return a_; }
    D3dDeviceChannel &b() { return b_; }
    D3dDeviceChannel &twelve() { return twelve_; }

    /* The ids of what the last batch carried, and each SelectDevice11's device. */
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

}  // namespace

TEST(d3d_device_channel, a_device_is_selected_only_when_another_spoke_last)
{ Fixture fixture;
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(rvk_d3d_Draw(fixture.a(), 6u, 0u));
  CHECK(rvk_d3d_Draw(fixture.b(), 9u, 0u));
  CHECK(rvk_d3d_Draw(fixture.a(), 12u, 0u));
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
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(!fixture.lock().held());
  CHECK(rvk_d3d_Draw(fixture.b(), 3u, 0u));
  CHECK_STATUS(fixture.b().flush(), Status::ok);
  CHECK(fixture.b().make_room_for(0u) == fixture.transport().blobs().ready());
  CHECK(!fixture.lock().held());
  CHECK(std::strcmp(fixture.trace().text(), "PpPpPpPp") == 0); }

/* The device's answer is written before the call reads it: the first batch
 * that asks for one is fence 1. */
TEST(d3d_device_channel, a_synchronous_call_keeps_the_lock_until_it_ends)
{ Fixture fixture;
  std::uint8_t *reply = fixture.kmt().reply_of(fixture.transport().context());
  CHECK(reply != nullptr);
  static_cast<void>(av::ReplyHeader(1u, 4u).encode(reply, av::ReplyHeader::encoded_size));
  const std::uint32_t removed = 0x887A0005u;
  std::memcpy(reply + av::ReplyHeader::encoded_size, &removed, sizeof removed);
  std::uint32_t answered = 0u;
  CHECK(rvk_d3d_Finish11(fixture.a(), &answered));
  CHECK_EQ(answered, removed);
  CHECK(!fixture.lock().held());
  CHECK(std::strcmp(fixture.trace().text(), "Pp") == 0); }

TEST(d3d_device_channel, a_command_given_up_before_it_was_sent_lets_the_lock_go)
{ Fixture fixture;
  const Box whole = {};
  /* Four bytes named with no data behind them: refused while encoding. */
  CHECK(!rvk_d3d_UpdateSubresource(fixture.a(), 1u, 0u, 0u, whole, 0u, 0u, 0u, nullptr, 4u));
  CHECK(!fixture.lock().held());
  CHECK(rvk_d3d_Draw(fixture.b(), 3u, 0u));
  CHECK(std::strcmp(fixture.trace().text(), "PpPp") == 0); }

TEST(d3d_device_channel, closing_destroys_the_device_and_the_next_speaker_is_selected_again)
{ Fixture fixture;
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(fixture.a().close());
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  std::uint32_t ids[8] = {}, selected[8] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 5u);
  CHECK_EQ(ids[2], kCmd_DestroyDevice11);
  CHECK_EQ(ids[3], kCmd_SelectDevice11);
  CHECK_EQ(selected[3], device_a);
  CHECK(!fixture.lock().held()); }

/* A stream that failed fails every later command, the selection included, and
 * the channel still lets the lock go. */
TEST(d3d_device_channel, a_failed_stream_fails_the_command_and_keeps_no_lock)
{ Fixture fixture;
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::invalid_state);
  CHECK(fixture.stream().failed());
  CHECK(!rvk_d3d_Draw(fixture.b(), 3u, 0u));
  CHECK(!fixture.b().close());
  CHECK(!fixture.lock().held()); }

namespace {

/* The host's answer to the first batch that asks for one: an hresult and a level. */
void answer_create(Fixture &fixture, std::uint32_t hresult)
{
    std::uint8_t *reply = fixture.kmt().reply_of(fixture.transport().context());
    CHECK(reply != nullptr);
    static_cast<void>(av::ReplyHeader(1u, 8u).encode(reply, av::ReplyHeader::encoded_size));
    const std::uint32_t words[2] = {hresult, 0xb100u};
    std::memcpy(reply + av::ReplyHeader::encoded_size, words, sizeof words);
}

}  // namespace

/* CreateDevice11 selects the device on the host: sent after a selection of an
 * id the host does not know yet, the selection would be refused and the
 * session lost. So opening sends no selection, and the device's next command
 * needs none. */
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
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK_STATUS(fixture.a().flush(), Status::ok);
  CHECK_EQ(fixture.shipped(ids, selected, 4u), 1u);
  CHECK_EQ(ids[0], kCmd_Draw);
  CHECK(!fixture.lock().held()); }

/* A device the host would not make is not the one it has selected. */
TEST(d3d_device_channel, a_refused_open_leaves_the_next_command_to_select)
{ Fixture fixture;
  answer_create(fixture, 0x80004005u);
  std::uint32_t hresult = 0u, level = 0u;
  CHECK(fixture.a().open(0xb100u, hresult, level));
  CHECK_EQ(hresult, 0x80004005u);
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
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

/* A Direct3D 12 device speaks as device 0: its commands name their own
 * objects, so nothing is selected before them, and the device a Direct3D 11
 * command selected stays selected across them - the next command from it
 * needs no selection either. */
TEST(d3d_device_channel, a_direct3d_12_speaker_selects_nothing_and_keeps_the_selection)
{ Fixture fixture;
  CHECK(rvk_d3d12_DrawInstanced12(fixture.twelve(), 1u, 3u, 1u, 0u, 0u));
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(rvk_d3d12_DrawInstanced12(fixture.twelve(), 1u, 3u, 1u, 0u, 0u));
  CHECK(rvk_d3d_Draw(fixture.a(), 6u, 0u));
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

/* A deferred context's channel: what it recorded is counted and taken out of
 * the stream under the process's lock, and an application's Flush is the
 * stream's, under the lock too. */
TEST(d3d_device_channel, a_recording_is_counted_and_taken_under_the_lock)
{ Fixture fixture;
  av::MockPlatform platform;
  av::d3d::CommandRecording recording(platform, block_bytes, block_bytes);
  CHECK_STATUS(recording.initialize(), Status::ok);
  fixture.stream().record(recording);
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  fixture.trace().clear();
  const std::size_t recorded = fixture.a().recorded_bytes();
  /* The selection of a, then its draw. */
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
  CHECK(rvk_d3d_Draw(fixture.a(), 3u, 0u));
  fixture.trace().clear();
  CHECK_STATUS(fixture.a().application_flush(), Status::ok);
  CHECK(!fixture.lock().held());
  CHECK(std::strcmp(fixture.trace().text(), "Pp") == 0);
  std::uint32_t ids[8] = {}, selected[8] = {};
  CHECK_EQ(fixture.shipped(ids, selected, 8u), 3u);
  CHECK_EQ(ids[1], kCmd_Draw);
  CHECK_EQ(ids[2], kCmd_Flush11);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u); }
