/* One driver context's way to the host (HostChannel): the staging a command
 * is written in, the transport, the batch and the blob arena, opened all or
 * nothing and closed in the one order that is safe. What every OpenGL,
 * OpenCL, Direct3D and Vulkan context is opened over, so each answer here is
 * one all four give. */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/host_channel_probe.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace av {

/* A transport whose established context has lost its name: what its
 * invariant is there to catch. */
struct KmtTransportProbe {
    static std::uint32_t forget_name(KmtTransport &t)
    {
        const std::uint32_t name = t.allocations_[0];
        t.allocations_[0] = 0u;
        return name;
    }
    static void restore_name(KmtTransport &t, std::uint32_t name) { t.allocations_[0] = name; }
};

}  // namespace av

using av::GuestConnection;
using av::KmtBlobArena;
using av::KmtTransportProbe;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::ChannelShape;
using av::icd::HostChannel;
using av::icd::HostChannelProbe;

namespace {

const PciIdentity avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint8_t  ring_order = 12u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 4096u;
constexpr std::uint32_t command_id = 0x10001u;
/* What the device answers an import it will not take. */
constexpr std::uint32_t import_refused = 1u;
constexpr std::uint64_t imported_memory = 0x0000600000000077ull;

/* OpenGL's and OpenCL's shape: batched, with an arena twice the least. */
ChannelShape batching()
{
    return ChannelShape(SubApi::opengl, ring_order, reply_bytes, staging_bytes, 2u * staging_bytes,
                        2u * KmtBlobArena::default_bytes);
}

/* Vulkan's: each command alone, and no arena. */
ChannelShape plain()
{
    return ChannelShape(SubApi::vulkan, ring_order, reply_bytes, staging_bytes, 0u, 0u);
}

class Rig {
public:
    Rig() : kmt_(), platform_(), connection_(kmt_), host_(kmt_), log_(), linked_(0u)
    {
        kmt_.add_adapter(avgpu, 3200u);
        std::uint32_t adapter = 0u, count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter), Status::ok);
        linked_ = kmt_.open_objects();
    }
    ~Rig() { CHECK_EQ(kmt_.misuses(), 0u); }

    MockKmt              &kmt() { return kmt_; }
    av::MockPlatform     &platform() { return platform_; }
    GuestConnection      &device() { return connection_; }
    av::ScriptedHost     &host() { return host_; }
    av::RecordingLog     &log() { return log_; }

    /* Only the device link is left: no transport object, no chunk and no
     * block of the channel's. */
    void check_nothing_held(const HostChannel &channel)
    {
        CHECK(!channel.opened());
        CHECK(!channel.faulted());
        CHECK(channel.arena_view() == nullptr);
        CHECK_EQ(channel.arena_bytes(), 0u);
        CHECK_EQ(kmt_.open_objects(), linked_);
        CHECK_EQ(platform_.live_allocations(), 0u);
        CHECK(channel.invariants_hold());
    }

    /* A command of a few bytes through `wire` that wants no answer. */
    TransportResult send(av::ClientTransport &wire)
    {
        WireWriter writer = wire.request_writer();
        writer.write_zeros(8u);
        return wire.send_async(command_id, writer);
    }

private:
    MockKmt          kmt_;
    av::MockPlatform platform_;
    GuestConnection  connection_;
    av::ScriptedHost host_;
    av::RecordingLog log_;
    std::uint32_t    linked_;
};

const char *report_of(const HostChannel &channel, char *bytes, std::size_t size)
{
    av::StateWriter writer(bytes, size);
    writer.open("channel", 0u);
    channel.report(writer);
    return writer.dump();
}

}  // namespace

TEST(host_channel, a_closed_channel_holds_nothing_and_closing_it_again_is_nothing)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.check_nothing_held(channel);
  CHECK_EQ(channel.host_errors(), 0u);
  channel.close(false);
  channel.close(true);
  rig.check_nothing_held(channel);
  char bytes[128];
  CHECK(std::strcmp(report_of(channel, bytes, sizeof bytes), "channel/0 opened=false faulted=false arena_bytes=0") ==
        0);
  CHECK_EQ(rig.log().lines(), 0u); }

TEST(host_channel, opening_holds_the_transport_the_batch_and_the_arena_at_the_shapes_size)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(channel.opened());
  CHECK(!channel.faulted());
  CHECK_EQ(channel.arena_bytes(), 2u * KmtBlobArena::default_bytes);
  CHECK(channel.arena_view() != nullptr);
  /* Bulk data is lent from the arena through the transport it was opened on. */
  CHECK(channel.transport().blobs().ready());
  CHECK(channel.invariants_hold());
  char bytes[128];
  CHECK(std::strcmp(report_of(channel, bytes, sizeof bytes),
                    "channel/0 opened=true faulted=false arena_bytes=33554432") == 0);
  /* A second open is refused and leaves the first as it was. */
  CHECK_STATUS(channel.open(rig.device()), Status::invalid_state);
  CHECK(channel.opened());
  CHECK_EQ(channel.arena_bytes(), 2u * KmtBlobArena::default_bytes);
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel);
  CHECK_EQ(rig.log().lines(), 0u); }

/* The host writes its error word in the arena's last bytes, which it never
 * lends; the channel reads it from there. */
TEST(host_channel, the_host_error_word_is_the_arenas_tail)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK_EQ(channel.host_errors(), 0u);
  const std::uint64_t word = 0x0000000500000502ull;
  std::memcpy(channel.arena_view() + channel.arena_bytes() - sizeof word, &word, sizeof word);
  CHECK_EQ(channel.host_errors(), word);
  rig.host().nop();
  channel.close(false); }

TEST(host_channel, what_is_batched_reaches_the_host_before_its_context_goes)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.send(channel.batched()) == TransportResult::ok);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().nop();
  channel.close(false);
  /* The held command first, then the arena's release. */
  CHECK(rig.kmt().commands() > sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), command_id);
  rig.check_nothing_held(channel); }

/* A lost context's device frees what it held with the context: asking a
 * faulted transport would only fail, so nothing is sent. */
TEST(host_channel, a_lost_channel_closes_without_asking_the_host_anything)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(rig.send(channel.batched()) == TransportResult::ok);
  const std::uint32_t sent = rig.kmt().commands();
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  channel.close(true);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  rig.check_nothing_held(channel);
  CHECK_EQ(rig.log().lines(), 0u); }

/* Each size is the host window's when it has room and guest RAM's when not;
 * one neither can hold is asked again at half the size. */
TEST(host_channel, an_arena_the_device_refuses_is_asked_again_at_half_the_size)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.host().blob_import(import_refused, 0u);
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK_EQ(channel.arena_bytes(), KmtBlobArena::default_bytes);
  CHECK(channel.arena_view() != nullptr);
  CHECK_EQ(rig.log().holding("no blob arena"), 0u);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel); }

/* Never below the least an arena is opened at: past it the channel opens
 * without one, and its bulk data crosses inline. */
TEST(host_channel, a_device_that_will_not_import_at_the_least_size_leaves_a_channel_without_an_arena)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.host().blob_import(import_refused, 0u);
  rig.host().blob_import(import_refused, 0u);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(channel.opened());
  CHECK(channel.arena_view() == nullptr);
  CHECK_EQ(channel.arena_bytes(), 0u);
  CHECK_EQ(channel.host_errors(), 0u);
  CHECK(!channel.transport().blobs().ready());
  CHECK_EQ(rig.log().holding("avtest: no blob arena for the context; bulk data crosses inline"), 1u);
  CHECK(rig.send(channel.batched()) == TransportResult::ok);
  CHECK(channel.invariants_hold());
  channel.close(false);
  rig.check_nothing_held(channel); }

/* A host that says nothing to the import faults the transport: a context
 * whose every call is already lost is not one to hand back - whether that is
 * the first ask or the one at half the size. */
TEST(host_channel, a_transport_that_faults_opening_the_arena_opens_nothing)
{ for (std::uint32_t refusals = 0u; refusals < 2u; refusals++) {
      Rig rig;
      HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
      if (refusals != 0u) rig.host().blob_import(import_refused, 0u);
      CHECK_STATUS(channel.open(rig.device()), Status::invalid_state);
      CHECK_EQ(rig.log().holding("avtest: the context faulted opening its blob arena"), 1u);
      rig.check_nothing_held(channel);
  } }

TEST(host_channel, no_memory_for_the_staging_or_the_batch_opens_nothing)
{ for (std::uint32_t failing = 1u; failing <= 2u; failing++) {
      Rig rig;
      HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
      rig.platform().fail_allocation_at(failing);
      CHECK_STATUS(channel.open(rig.device()), Status::no_memory);
      rig.check_nothing_held(channel);
  } }

TEST(host_channel, a_driver_that_refuses_the_context_opens_nothing)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", plain());
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, 0xC0000001u);
  CHECK(channel.open(rig.device()) != Status::ok);
  CHECK_EQ(rig.log().holding("avtest: the driver refused the context"), 1u);
  rig.check_nothing_held(channel); }

/* Vulkan's shape: nothing held between calls, and nothing to import. */
TEST(host_channel, a_shape_that_neither_batches_nor_lends_opens_the_transport_alone)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", plain());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(channel.opened());
  CHECK(channel.arena_view() == nullptr);
  CHECK_EQ(rig.kmt().commands(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 1u);
  CHECK(channel.invariants_hold());
  channel.close(false);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_transport_the_host_failed_is_a_faulted_channel)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", plain());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  WireWriter writer = channel.transport().request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(channel.transport().send_sync(command_id, writer, reply) != TransportResult::ok);
  CHECK(channel.faulted());
  channel.close(true);
  rig.check_nothing_held(channel); }

/* A host that never answers the arena's release is asked nothing more: what
 * the channel still holds goes with the context. */
TEST(host_channel, an_arena_that_does_not_close_whole_is_logged_and_closed_anyway)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.log(), "avtest: ", batching());
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  channel.close(false);
  CHECK_EQ(rig.log().holding("avtest: the blob arena did not close whole"), 1u);
  CHECK(!channel.opened());
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(host_channel, the_invariant_catches_every_state_the_channel_never_builds)
{ Rig rig;
  HostChannel      channel(rig.platform(), rig.log(), "avtest: ", batching());
  HostChannelProbe probe(channel);
  std::uint8_t     stray = 0u;
  /* Closed, holding a block of either kind. */
  CHECK(probe.swap_staging(&stray) == nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_staging(nullptr) == &stray);
  CHECK(probe.swap_batch_block(&stray) == nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_batch_block(nullptr) == &stray);
  CHECK(channel.invariants_hold());
  /* Open, with no staging, a batching shape without its batch, a batch
   * without its block, and a transport that is unsound. */
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  std::uint8_t *const staging = probe.swap_staging(nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_staging(staging) == nullptr);
  CHECK(channel.invariants_hold());
  probe.drop_batched();
  CHECK(!channel.invariants_hold());
  probe.restore_batched();
  CHECK(channel.invariants_hold());
  std::uint8_t *const block = probe.swap_batch_block(nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_batch_block(block) == nullptr);
  CHECK(channel.invariants_hold());
  const std::uint32_t name = KmtTransportProbe::forget_name(channel.transport());
  CHECK(!channel.invariants_hold());
  KmtTransportProbe::restore_name(channel.transport(), name);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel); }

/* What a closed channel can be found holding: a transport no open made. */
TEST(host_channel, a_transport_no_open_made_is_an_open_channel_with_no_staging)
{ Rig rig;
  HostChannel      channel(rig.platform(), rig.log(), "avtest: ", plain());
  HostChannelProbe probe(channel);
  probe.build_transport(rig.device());
  CHECK(channel.opened());
  CHECK(!channel.invariants_hold());
  probe.drop_transport();
  rig.check_nothing_held(channel); }
