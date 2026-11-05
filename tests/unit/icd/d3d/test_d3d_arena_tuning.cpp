#include "core/log/driver_log.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"

#include <cstdint>

using av::KmtBlobArena;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using av::SubApi;
using av::d3d::D3dCommandStream;
using namespace av::d3dproto;

namespace av {

struct KmtTransportProbe {
    static void          press(KmtTransport &transport, std::uint32_t waits) { transport.room_pressure_ += waits; }
    static std::uint32_t pressure(const KmtTransport &transport) { return transport.room_pressure_; }
};

}

using av::KmtTransportProbe;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t imported_memory = 0x0000600000000077ull;
constexpr std::uint64_t window_bytes = std::uint64_t{256} << 20u;
constexpr std::uint8_t  ring_order = 20u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::size_t   block_bytes = D3dCommandStream::command_room + 4096u;
constexpr std::uint64_t frame_step = MockKmt::ticks_per_second / 100u;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), host_(kmt_), staging_{}, block_{},
          transport_(connection_, SubApi::d3d, ring_order, reply_bytes, staging_, staging_bytes), sink_(),
          lines_(sink_, "test"), arena_(connection_, transport_, lines_), stream_(transport_)
    {
        kmt_.add_adapter(av, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        transport_.hold_in(block_, block_bytes);
        kmt_.open_host_window(window_bytes);
        host_.blob_import(0u, imported_memory);
        host_.blob_import(0u, imported_memory + 1u);
        CHECK_STATUS(arena_.open(0u), Status::ok);
    }
    ~Rig()
    {
        host_.nop();
        host_.nop();
        host_.nop();
        CHECK_STATUS(arena_.close(), Status::ok);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    MockKmt          &kmt() { return kmt_; }
    ScriptedHost     &host() { return host_; }
    KmtTransport     &transport() { return transport_; }
    KmtBlobArena     &arena() { return arena_; }
    D3dCommandStream &stream() { return stream_; }
    av::RecordingLog &log() { return sink_; }

    void fill_the_ring()
    {
        av::ClientBlobArena &blobs = transport_.blobs();
        CHECK(blobs.take(blobs.room()).taken());
    }

    void a_frame_of_waits(std::uint32_t waits)
    {
        KmtTransportProbe::press(transport_, waits);
        CHECK(av_d3d_Draw(stream_, 3u, 0u));
        CHECK_STATUS(stream_.application_flush(), Status::ok);
        CHECK_STATUS(stream_.flush(), Status::ok);
    }

private:
    MockKmt          kmt_;
    std::uint32_t    adapter_;
    av::GuestConnection connection_;
    ScriptedHost     host_;
    std::uint8_t     staging_[staging_bytes];
    std::uint8_t     block_[block_bytes];
    KmtTransport     transport_;
    av::RecordingLog sink_;
    av::DriverLog    lines_;
    KmtBlobArena     arena_;
    D3dCommandStream stream_;
};

}

TEST(d3d_arena_tuning, waits_spread_over_frames_of_fewer_than_four_each_still_grow_the_ring)
{ Rig rig;
  rig.fill_the_ring();
  rig.host().blob_import(0u, imported_memory + 2u);
  rig.kmt().reads_clock(nullptr, frame_step);
  rig.a_frame_of_waits(2u);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  rig.a_frame_of_waits(2u);
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 0u);
  CHECK_EQ(rig.log().lines(), 0u);
  CHECK(rig.arena().invariants_hold());
  CHECK(rig.stream().invariants_hold());
  rig.host().nop(); }

TEST(d3d_arena_tuning, an_empty_flush_between_frames_does_not_forget_the_waits)
{ Rig rig;
  rig.fill_the_ring();
  rig.host().blob_import(0u, imported_memory + 2u);
  rig.kmt().reads_clock(nullptr, frame_step);
  KmtTransportProbe::press(rig.transport(), 3u);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_STATUS(rig.stream().application_flush(), Status::ok);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 3u);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  KmtTransportProbe::press(rig.transport(), 1u);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.host().nop(); }

TEST(d3d_arena_tuning, waits_older_than_a_second_are_forgotten_and_a_calm_ring_grows_nothing)
{ Rig rig;
  rig.fill_the_ring();
  rig.kmt().reads_clock(nullptr, 4u * MockKmt::ticks_per_second);
  KmtTransportProbe::press(rig.transport(), 3u);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 0u);
  KmtTransportProbe::press(rig.transport(), 1u);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK_EQ(rig.log().lines(), 0u); }

TEST(d3d_arena_tuning, an_idle_segment_is_released_by_flushes_with_nothing_to_send)
{ Rig rig;
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.kmt().reads_clock(nullptr, 4u * MockKmt::ticks_per_second);
  CHECK_EQ(rig.stream().recorded_bytes(), 0u);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.host().nop();
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK(rig.arena().invariants_hold());
  rig.host().nop(); }

TEST(d3d_arena_tuning, an_application_flush_with_nothing_recorded_releases_an_idle_segment)
{ Rig rig;
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  rig.kmt().reads_clock(nullptr, 4u * MockKmt::ticks_per_second);
  CHECK_STATUS(rig.stream().application_flush(), Status::ok);
  CHECK_STATUS(rig.stream().application_flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.host().nop();
  CHECK_STATUS(rig.stream().application_flush(), Status::ok);
  CHECK_STATUS(rig.stream().application_flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  rig.host().nop(); }

TEST(d3d_arena_tuning, a_flush_with_commands_held_still_releases_an_idle_segment)
{ Rig rig;
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  rig.kmt().reads_clock(nullptr, 4u * MockKmt::ticks_per_second);
  CHECK(av_d3d_Draw(rig.stream(), 1u, 0u));
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.host().nop();
  CHECK(av_d3d_Draw(rig.stream(), 2u, 0u));
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  rig.host().nop(); }

TEST(d3d_arena_tuning, a_grown_segment_that_held_published_blobs_is_released_once_the_host_consumed_them)
{ Rig rig;
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  CHECK(rig.transport().blobs().take(18u * mib).taken());
  CHECK(av_d3d_Draw(rig.stream(), 1u, 0u));
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  rig.host().nop();
  CHECK_STATUS(rig.transport().await_gpu(rig.transport().published_fence()), Status::ok);
  rig.host().nop();
  CHECK(rig.transport().blobs().used() != 0u);
  rig.kmt().reads_clock(nullptr, 4u * MockKmt::ticks_per_second);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_STATUS(rig.stream().flush(), Status::ok);
  CHECK_EQ(rig.transport().blobs().used(), 0u);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK(rig.arena().invariants_hold());
  rig.host().nop(); }
