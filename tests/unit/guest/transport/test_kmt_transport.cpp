#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
#include "guest/transport/blob_import.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_watch.hpp"
#include "guest/transport/channel_stats.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "guest/transport/publish_gate.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/framed_block_probe.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::BatchHeader;
using av::BatchMark;
using av::CommandHeader;
using av::GuestConnection;
using av::KmtTransport;
using av::DoorbellStatus;
using av::MockKmt;
using av::PciIdentity;
using av::ReplyHeader;
using av::AvContextRecord;
using av::Status;
using av::SubApi;
using av::SubmissionDescriptor;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;

namespace av {

struct KmtTransportProbe {
    static std::uint32_t name_allocation(const KmtTransport &t) { return t.allocations_[0]; }
    static std::uint32_t dma_allocation(const KmtTransport &t) { return t.allocations_[1]; }
    static void orphan(KmtTransport &t) { t.context_ = 1u; t.dma_address_ = 0u; }
    static void nameless(KmtTransport &t) { t.context_ = 1u; t.dma_address_ = 0x1000u; t.allocations_[0] = 0u; }
    static void overhold(KmtTransport &t) { FramedBlockProbe(t.pending_).used() = t.pending_.capacity() + 1u; }
};

}

using av::KmtTransportProbe;

namespace {

bool room_for(av::KmtTransport &transport, std::uint64_t bytes)
{
    const std::uint64_t wanted[1] = {bytes};
    return transport.make_room_for_blobs(wanted, 1u);
}

std::uint32_t sent_commands(const av::MockKmt &kmt)
{
    std::uint32_t counted = 0u;
    for (std::uint32_t index = 0u; index < kmt.commands(); index++)
        if (kmt.command_id(index) != CommandHeader::channel_stats_id) counted++;
    return counted;
}

std::uint32_t last_sent_command(const av::MockKmt &kmt)
{
    for (std::uint32_t index = kmt.commands(); index != 0u; index--)
        if (kmt.command_id(index - 1u) != CommandHeader::channel_stats_id) return kmt.command_id(index - 1u);
    return 0u;
}

constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;

const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);


std::uint32_t found_adapter(MockKmt &kmt)
{
    std::uint32_t adapters[1] = {};
    std::uint32_t count = 0u;
    static_cast<void>(kmt.enumerate_adapters(adapters, 1u, count));
    return adapters[0];
}

class Fixture {
public:
    Fixture() : connection_(kmt_) { kmt_.add_adapter(av_gpu, 3200u); }
    MockKmt         &kmt() { return kmt_; }
    GuestConnection &connection() { return connection_; }
    Status open() { return connection_.open(found_adapter(kmt_)); }

private:
    MockKmt         kmt_;
    GuestConnection connection_;
};

}

TEST(kmt_transport, submits_through_the_kernel_without_a_doorbell_and_through_one_with_it)
{ { Fixture fixture;
    CHECK_STATUS(fixture.open(), Status::ok);
    std::uint8_t transport_staging[staging_bytes] = {};
    KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, transport_staging,
                           sizeof transport_staging);
    CHECK_STATUS(transport.initialize(), Status::ok);
    WireWriter writer = transport.request_writer();
    writer.write_u32(7u);
    CHECK(transport.send_async(1u, writer) == av::TransportResult::ok);
    CHECK_EQ(transport.submissions_through_kernel(), 1u);
    CHECK_EQ(transport.submissions_through_doorbell(), 0u);
    CHECK_EQ(fixture.kmt().calls(MockKmt::Call::create_doorbell), 0u);
    transport.teardown(); }

  Fixture fixture;
  fixture.kmt().allow_hardware_scheduling();
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint32_t submitted_before = fixture.kmt().calls(MockKmt::Call::submit);
  WireWriter          writer = transport.request_writer();
  writer.write_u32(7u);
  CHECK(transport.send_async(1u, writer) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_doorbell(), 1u);
  CHECK_EQ(transport.submissions_through_kernel(), 0u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), submitted_before);
  transport.teardown();
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_doorbell), 1u); }

TEST(kmt_transport, the_newest_batch_is_marked_for_another_thread_to_wait_on)
{ for (const bool hardware : {false, true}) {
    Fixture fixture;
    if (hardware) fixture.kmt().allow_hardware_scheduling();
    CHECK_STATUS(fixture.open(), Status::ok);
    std::uint8_t transport_staging[staging_bytes] = {};
    KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                           sizeof transport_staging);
    CHECK_STATUS(transport.initialize(), Status::ok);
    const BatchMark none = transport.newest_mark();
    CHECK(!none.waitable());
    CHECK_STATUS(none.wait(fixture.kmt()), Status::invalid_state);
    for (std::uint32_t batch = 1u; batch <= 2u; batch++) {
      WireWriter writer = transport.request_writer();
      writer.write_u32(7u);
      CHECK(transport.send_async(1u, writer) == TransportResult::ok);
    }
    const BatchMark mark = transport.newest_mark();
    CHECK(mark.waitable());
    CHECK_EQ(mark.fence(), 2u);
    const MockKmt::Call waited = hardware ? MockKmt::Call::wait_for_hw_queue : MockKmt::Call::wait_for_context;
    const std::uint32_t before = fixture.kmt().calls(waited);
    CHECK_STATUS(mark.wait(fixture.kmt()), Status::ok);
    CHECK_EQ(fixture.kmt().calls(waited) - before, 1u);
    transport.teardown();
  } }

TEST(kmt_transport, is_unestablished_until_initialized)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK(!transport.established());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, context_is_the_virtual_context_while_established)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_EQ(transport.context(), 0u);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint32_t context = transport.context();
  CHECK(context != 0u);
  CHECK_STATUS(fixture.kmt().wait_for_context(fixture.connection().device(), context), Status::ok);
  CHECK_EQ(fixture.kmt().misuses(), 0u);
  transport.teardown();
  CHECK_EQ(transport.context(), 0u);
  CHECK(transport.invariants_hold());
  CHECK_STATUS(fixture.kmt().wait_for_context(fixture.connection().device(), context), Status::invalid_parameter);
  CHECK_EQ(fixture.kmt().misuses(), 1u); }

TEST(kmt_transport, initialize_names_the_context_and_takes_the_driver_s_views)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.established());
  CHECK_EQ(fixture.kmt().last_private_size(), AvContextRecord::encoded_size);
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::name_allocation(transport)), 17u);
  CHECK_EQ(fixture.kmt().allocation_pages(KmtTransportProbe::name_allocation(transport)), 1u);
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::dma_allocation(transport)), 16u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::lock), 0u);
  AvContextRecord record;
  CHECK_STATUS(record.decode(fixture.kmt().last_private(), fixture.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.ring(), KmtTransportProbe::name_allocation(transport));
  CHECK_EQ(record.ring_order(), 12u);
  CHECK_EQ(record.reply_bytes(), reply_bytes);
  const std::uint8_t *const ring = fixture.kmt().ring_of(transport.context());
  CHECK(ring != nullptr);
  av::RingHeader header;
  CHECK_STATUS(header.decode(ring, av::RingHeader::encoded_size), Status::ok);
  CHECK_EQ(header.order(), 12u);
  CHECK_STATUS(transport.initialize(), Status::invalid_state);
  CHECK(transport.invariants_hold());
  transport.teardown();
  CHECK(!transport.established()); }

TEST(kmt_transport, an_opengl_record_carries_its_options_and_the_ring_is_named)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_EQ(transport.ring(), 0u);
  const av::GlContextOptions options(4u, 5u, av::GlContextOptions::Profile::core,
                                     av::GlContextOptions::debug, 0x4242u);
  CHECK_STATUS(transport.initialize(options), Status::ok);
  CHECK_EQ(transport.ring(), KmtTransportProbe::name_allocation(transport));
  AvContextRecord record;
  CHECK_STATUS(record.decode(fixture.kmt().last_private(), fixture.kmt().last_private_size()), Status::ok);
  const av::GlContextOptions carried = record.options();
  CHECK_EQ(carried.major(), 4u);
  CHECK_EQ(carried.minor(), 5u);
  CHECK(carried.profile() == av::GlContextOptions::Profile::core);
  CHECK_EQ(carried.flags(), av::GlContextOptions::debug);
  CHECK_EQ(carried.share_context(), 0x4242u);
  CHECK(transport.invariants_hold());
  transport.teardown();
  CHECK_EQ(transport.ring(), 0u);
  KmtTransport vulkan(fixture.connection(), SubApi::vulkan, 12u,
                      reply_bytes, transport_staging, sizeof transport_staging);
  const std::uint32_t allocations = fixture.kmt().calls(MockKmt::Call::create_allocation);
  CHECK_STATUS(vulkan.initialize(options), Status::invalid_parameter);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::create_allocation), allocations);
  CHECK(!vulkan.established());
  const av::GlContextOptions invalid(4u, 7u, av::GlContextOptions::Profile::core, 0u, 0u);
  CHECK_STATUS(transport.initialize(invalid), Status::invalid_parameter);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::create_allocation), allocations);
  CHECK_EQ(fixture.kmt().open_objects(), 3u); }

TEST(kmt_transport, an_async_command_lands_a_batch_and_a_submission)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  WireWriter writer = transport.request_writer();
  writer.write_u32(0xCAFEu);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  const std::uint8_t *ring = fixture.kmt().ring_of(transport.context());
  CHECK(ring != nullptr);
  BatchHeader batch;
  CHECK_STATUS(batch.decode(ring + 32u, BatchHeader::encoded_size), Status::ok);
  CHECK_EQ(batch.fence_id(), 1u);
  CommandHeader command;
  CHECK_STATUS(command.decode(ring + 32u + BatchHeader::encoded_size, CommandHeader::encoded_size), Status::ok);
  CHECK_EQ(command.id(), 0x10001u);
  CHECK_EQ(fixture.kmt().last_submission_size(), av::SubmissionDescriptor::encoded_size);
  av::SubmissionDescriptor submitted;
  CHECK_STATUS(submitted.decode(fixture.kmt().last_submission(), av::SubmissionDescriptor::encoded_size), Status::ok);
  CHECK_EQ(submitted.fence_id(), 1u);
  CHECK_EQ(submitted.begin(), 0u);
  CHECK_EQ(submitted.end(), 48u);   }

TEST(kmt_transport, an_overflowed_writer_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  WireWriter writer = transport.request_writer();
  writer.write_zeros(staging_bytes + 1u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed); }

TEST(kmt_transport, a_sync_command_reads_the_reply_the_device_wrote)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint8_t *reply = fixture.kmt().reply_of(transport.context());
  CHECK(reply != nullptr);
  static_cast<void>(ReplyHeader(1u, 8u).encode(reply, ReplyHeader::encoded_size));
  const std::uint8_t answer[8] = {1u, 0u, 0u, 0u, 2u, 0u, 0u, 0u};
  std::memcpy(reply + ReplyHeader::encoded_size, answer, sizeof answer);
  WireWriter writer = transport.request_writer();
  writer.write_u32(7u);
  WireReader got(nullptr, 0u);
  CHECK(transport.send_sync(0x10002u, writer, got) == TransportResult::ok);
  CHECK_EQ(got.read_u32(), 1u);
  transport.end_call();
  CHECK_EQ(got.read_u32(), 2u);
  CHECK_EQ(fixture.kmt().commands(), 1u); }

TEST(kmt_transport, an_answered_call_retires_every_blob_lease_and_nothing_else_does)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint8_t room[64] = {};
  transport.blobs().lend(room, sizeof room);
  CHECK(transport.blobs().take(16u).taken());
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 16u);
  WireWriter async = transport.request_writer();
  async.write_u32(1u);
  CHECK(transport.send_async(0x30001u, async) == TransportResult::ok);
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 16u);
  const std::uint8_t nothing = 0u;
  fixture.kmt().queue_answer(&nothing, 0u);
  WireWriter sync = transport.request_writer();
  sync.write_u32(2u);
  WireReader got(nullptr, 0u);
  CHECK(transport.send_sync(0x30002u, sync, got) == TransportResult::ok);
  CHECK_EQ(transport.blobs().used(), 16u);
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 0u);
  CHECK(transport.blobs().take(8u).taken());
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 8u);
  fixture.kmt().withhold_answer_at(1u);
  WireWriter lost = transport.request_writer();
  lost.write_u32(3u);
  CHECK(transport.send_sync(0x30002u, lost, got) == TransportResult::failed);
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 8u);
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, room_for_a_lease_waits_for_the_oldest_batches_alone)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint8_t room[64] = {};
  transport.blobs().lend(room, sizeof room);
  const auto send_with_lease = [&](std::uint32_t value) {
      CHECK(transport.blobs().take(24u).taken());
      WireWriter writer = transport.request_writer();
      writer.write_u32(value);
      CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  };
  send_with_lease(1u);
  send_with_lease(2u);
  CHECK_EQ(transport.blobs().room(), 16u);
  CHECK(room_for(transport, 16u));
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 0u);
  CHECK(room_for(transport, 24u));
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 1u);
  CHECK(transport.blobs().room() >= 24u);
  CHECK(transport.blobs().take(transport.blobs().room()).taken());
  CHECK(!room_for(transport, 1u));
  CHECK(!transport.faulted());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, room_waited_for_while_the_head_lags_the_fence_comes_back)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint8_t room[32] = {};
  transport.blobs().lend(room, sizeof room);
  CHECK(transport.blobs().take(32u).taken());
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  fixture.kmt().consume_late_at(1u);
  CHECK(room_for(transport, 8u));
  CHECK(!transport.faulted());
  CHECK_EQ(transport.blobs().room(), 32u);
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, room_waited_for_from_a_host_that_stopped_faults_the_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint8_t room[32] = {};
  transport.blobs().lend(room, sizeof room);
  CHECK(transport.blobs().take(32u).taken());
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  fixture.kmt().fail_first(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  CHECK(!room_for(transport, 8u));
  CHECK(transport.faulted());
  CHECK(!room_for(transport, 8u)); }

TEST(kmt_transport, before_setup_and_the_refusing_arenas)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK(transport.decode_arena().allocate<std::uint8_t>(1u) == nullptr);
  CHECK(!transport.blobs().ready()); }

namespace {
void refuse_setup(MockKmt::Call call, std::uint32_t nth)
{
    MockKmt kmt;
    kmt.add_adapter(av_gpu, 3200u);
    GuestConnection connection(kmt);
    CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
    kmt.fail_at(call, nth, MockKmt::invalid_parameter);
    std::uint8_t transport_staging[staging_bytes] = {};
    KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
    CHECK(transport.initialize() != Status::ok);
    CHECK(!transport.established());
    CHECK(transport.invariants_hold());
}

}

TEST(kmt_transport, a_failure_at_each_setup_step_leaves_it_unestablished)
{ refuse_setup(MockKmt::Call::create_allocation, 1u);
  refuse_setup(MockKmt::Call::create_allocation, 2u);
  refuse_setup(MockKmt::Call::make_resident, 1u);
  refuse_setup(MockKmt::Call::wait_for_fence, 1u);
  refuse_setup(MockKmt::Call::map_gpu_virtual_address, 1u);
  refuse_setup(MockKmt::Call::create_context_virtual, 1u);
  MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  kmt.fail_first(MockKmt::Call::escape, 2u, MockKmt::invalid_parameter);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK(transport.initialize() != Status::ok);
  CHECK(!transport.established());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, a_failed_mapping_wait_leaves_it_unestablished)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t counted_staging[staging_bytes] = {};
  KmtTransport counted(connection, SubApi::vulkan, 12u, reply_bytes, counted_staging, sizeof counted_staging);
  CHECK_STATUS(counted.initialize(), Status::ok);
  const std::uint32_t waits = kmt.calls(MockKmt::Call::wait_for_fence);
  counted.teardown();
  MockKmt fresh;
  fresh.add_adapter(av_gpu, 3200u);
  GuestConnection other(fresh);
  CHECK_STATUS(other.open(found_adapter(fresh)), Status::ok);
  fresh.fail_at(MockKmt::Call::wait_for_fence, waits, MockKmt::invalid_parameter);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(other, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK(transport.initialize() != Status::ok);
  CHECK(!transport.established()); }

TEST(kmt_transport, a_torn_down_transport_initializes_again)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint32_t answered = 0u;
  fixture.kmt().answer_waits(nullptr, 0u);
  CHECK_STATUS(transport.nop(answered), Status::ok);
  CHECK_STATUS(transport.nop(answered), Status::ok);
  transport.teardown();
  CHECK(transport.invariants_hold());
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.established());
  CHECK_STATUS(transport.nop(answered), Status::ok);
  SubmissionDescriptor submitted;
  CHECK_STATUS(submitted.decode(fixture.kmt().last_submission(), fixture.kmt().last_submission_size()), Status::ok);
  CHECK_EQ(submitted.fence_id(), 1u);
  CHECK_EQ(submitted.begin(), 0u);
  transport.teardown();
  fixture.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  CHECK(transport.initialize() != Status::ok);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK_STATUS(transport.nop(answered), Status::ok);
  transport.teardown();
  CHECK_EQ(fixture.kmt().open_objects(), 3u);
  CHECK_EQ(fixture.kmt().misuses(), 0u); }

TEST(kmt_transport, views_the_driver_could_not_have_answered_are_refused)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  const std::uint64_t answers[][3] = {{0u, 0x2000u, 0x3000u},     {0x1000u, 0u, 0x3000u},
                                      {0x1000u, 0x1000u, 0x3000u}, {0x1000u, 0x2000u, 0u},
                                      {0x1000u, 0x2000u, 0x1000u}};
  for (const auto &views : answers) {
      std::uint8_t answer[av::ContextViewsRecord::encoded_size] = {};
      CHECK_STATUS(av::ContextViewsRecord().answered(views[0], views[1], views[2]).encode(answer, sizeof answer),
                   Status::ok);
      kmt.answer_next_escape(answer, sizeof answer);
      std::uint8_t transport_staging[staging_bytes] = {};
      KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging,
                             sizeof transport_staging);
      CHECK_STATUS(transport.initialize(), Status::malformed);
      CHECK(!transport.established());
      CHECK_EQ(kmt.open_objects(), 3u);
  } }

TEST(kmt_transport, the_fence_page_says_which_batches_the_gpu_finished)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.knows_gpu_progress());
  CHECK_EQ(transport.gpu_completed(), 0u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  CHECK_EQ(transport.gpu_completed(), 0u);
  std::uint32_t answered = 0u;
  fixture.kmt().answer_waits(nullptr, 0u);
  CHECK_STATUS(transport.nop(answered), Status::ok);
  CHECK_EQ(transport.gpu_completed(), 2u);
  CHECK_STATUS(transport.nop(answered), Status::ok);
  CHECK_EQ(transport.gpu_completed(), 3u);
  transport.teardown();
  CHECK(!transport.knows_gpu_progress());
  CHECK_EQ(transport.gpu_completed(), 0u); }

TEST(kmt_transport, a_wait_for_the_gpu_returns_once_the_page_shows_the_batch)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK_EQ(transport.published_fence(), 0u);
  CHECK_STATUS(transport.await_gpu(0u), Status::ok);
  CHECK_STATUS(transport.await_gpu(1u), Status::invalid_parameter);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  CHECK_EQ(transport.published_fence(), 1u);
  const std::uint32_t waits = fixture.kmt().calls(MockKmt::Call::wait_for_context);
  CHECK_STATUS(transport.await_gpu(1u), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), waits + 1u);
  CHECK(transport.gpu_completed() >= 1u);
  CHECK_STATUS(transport.await_gpu(1u), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), waits + 1u);
  WireWriter late = transport.request_writer();
  late.write_u32(2u);
  CHECK(transport.send_async(0x30001u, late) == TransportResult::ok);
  fixture.kmt().consume_late_at(1u);
  CHECK_STATUS(transport.await_gpu(2u), Status::ok);
  CHECK(!transport.faulted());
  WireWriter lost = transport.request_writer();
  lost.write_u32(3u);
  CHECK(transport.send_async(0x30001u, lost) == TransportResult::ok);
  fixture.kmt().fail_first(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  CHECK_STATUS(transport.await_gpu(3u), Status::invalid_state);
  CHECK(transport.faulted());
  CHECK_STATUS(transport.await_gpu(1u), Status::invalid_state); }

namespace {
class SlowDevice final : public av::ClockReader {
public:
    SlowDevice(MockKmt &kmt, std::uint32_t context, bool pulsing, std::uint64_t fence, std::uint32_t after)
        : kmt_(kmt), context_(context), pulsing_(pulsing), fence_(fence), after_(after), reads_(0u)
    {
    }

    void clock_read() override
    {
        reads_++;
        if (pulsing_) kmt_.pulse(context_);
        if (reads_ == after_) kmt_.complete_on_page(context_, fence_);
    }

    std::uint32_t reads() const { return reads_; }

private:
    MockKmt      &kmt_;
    std::uint32_t context_;
    bool          pulsing_;
    std::uint64_t fence_;
    std::uint32_t after_;
    std::uint32_t reads_;
};

constexpr std::uint64_t millisecond = MockKmt::ticks_per_second / 1000u;

}

TEST(kmt_transport, a_wait_for_an_older_batch_watches_the_page_for_as_long_as_the_pulse_beats)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  for (std::uint32_t i = 0u; i < 2u; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_u32(i);
      CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  }
  const std::uint32_t waits = fixture.kmt().calls(MockKmt::Call::wait_for_context);
  constexpr std::uint32_t two_minutes = 120000u;
  SlowDevice device(fixture.kmt(), transport.context(), true, 1u, two_minutes);
  fixture.kmt().reads_clock(&device, millisecond);
  CHECK_STATUS(transport.await_gpu(1u), Status::ok);
  fixture.kmt().reads_clock(nullptr, 0u);
  CHECK(device.reads() >= two_minutes);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), waits);
  CHECK_EQ(transport.gpu_completed(), 1u);
  CHECK(!transport.faulted()); }

TEST(kmt_transport, a_queue_waits_for_an_older_batch_through_its_progress_fence)
{ Fixture fixture;
  fixture.kmt().allow_hardware_scheduling();
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  for (std::uint32_t i = 0u; i < 2u; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_u32(i);
      CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  }
  const std::uint32_t waits = fixture.kmt().calls(MockKmt::Call::wait_for_hw_queue);
  CHECK_STATUS(transport.await_gpu(1u), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_hw_queue), waits + 1u);
  CHECK(transport.gpu_completed() >= 1u);
  CHECK(!transport.faulted()); }

TEST(kmt_transport, a_wait_on_a_silent_page_gives_up_after_the_silence)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  for (std::uint32_t i = 0u; i < 2u; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_u32(i);
      CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  }
  SlowDevice device(fixture.kmt(), transport.context(), false, 1u, 0u);
  fixture.kmt().reads_clock(&device, millisecond);
  CHECK_STATUS(transport.await_gpu(1u), Status::invalid_state);
  fixture.kmt().reads_clock(nullptr, 0u);
  const std::uint32_t silence = static_cast<std::uint32_t>(av::HostWatch::silence_seconds * 1000u);
  CHECK(device.reads() >= silence);
  CHECK(device.reads() < silence + 1000u);
  CHECK(transport.faulted()); }

TEST(kmt_transport, a_driver_with_no_fence_page_is_asked_again_and_reports_no_progress)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint32_t escapes = fixture.kmt().calls(MockKmt::Call::escape);
  transport.teardown();
  fixture.kmt().answer_views_up_to(av::ContextViewsRecord::without_fence);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::escape), escapes * 2u + 1u);
  CHECK(!transport.knows_gpu_progress());
  std::uint32_t answered = 0u;
  fixture.kmt().answer_waits(nullptr, 0u);
  CHECK_STATUS(transport.nop(answered), Status::ok);
  CHECK_EQ(transport.gpu_completed(), 0u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  const std::uint32_t waits = fixture.kmt().calls(MockKmt::Call::wait_for_context);
  CHECK_STATUS(transport.await_gpu(transport.published_fence()), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), waits + 1u); }

TEST(kmt_transport, a_refused_submission_fails_the_send)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  kmt.fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed); }

TEST(kmt_transport, a_full_fence_table_waits_for_the_host_before_the_next_send)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  for (std::uint32_t i = 0u; i < 64u; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_u32(i);
      CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  }
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), 0u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), 1u);
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, a_full_fence_table_whose_wait_fails_refuses_the_send)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  for (std::uint32_t i = 0u; i < 64u; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_u32(i);
      CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  }
  kmt.fail_first(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  WireWriter after = transport.request_writer();
  after.write_u32(65u);
  CHECK(transport.send_async(0x10001u, after) == TransportResult::failed); }

namespace {
void fill_the_fence_window(KmtTransport &transport)
{
    for (std::uint32_t i = 0u; i < 64u; i++) {
        WireWriter writer = transport.request_writer();
        writer.write_u32(i);
        CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
    }
}

constexpr std::uint64_t kernel_wait_seconds = 10u;
constexpr std::uint32_t silent_waits = static_cast<std::uint32_t>(av::HostWatch::silence_seconds / kernel_wait_seconds);

void bound_each_wait(MockKmt &kmt) { kmt.spend_on_wait(MockKmt::ticks_per_second * kernel_wait_seconds); }

}

TEST(kmt_transport, a_wait_that_times_out_while_the_host_works_is_waited_again)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  kmt.time_out_while_consuming(1u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 2u); }

TEST(kmt_transport, a_wait_that_times_out_with_nothing_moving_gives_up)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  bound_each_wait(kmt);
  kmt.time_out_silently(64u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 1u + silent_waits);
  CHECK(transport.faulted()); }

TEST(kmt_transport, a_host_silent_for_less_than_the_limit_is_waited_out)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  bound_each_wait(kmt);
  kmt.time_out_silently(silent_waits - 1u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + silent_waits);
  CHECK(!transport.faulted()); }

TEST(kmt_transport, a_host_that_stops_taking_batches_is_given_up_on_after_its_silence)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  bound_each_wait(kmt);
  kmt.time_out_while_consuming(64u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 1u + silent_waits); }

TEST(kmt_transport, a_wait_that_times_out_while_the_device_beats_is_waited_again)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  kmt.time_out_while_beating(3u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 4u);
  CHECK(!transport.faulted());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, a_heartbeat_that_stops_is_given_up_on_after_its_silence)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  bound_each_wait(kmt);
  kmt.time_out_while_beating(1u);
  kmt.time_out_silently(silent_waits);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 1u + silent_waits);
  CHECK(transport.faulted()); }

TEST(kmt_transport, a_wait_through_a_host_call_of_minutes_lasts_while_the_pulse_beats)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  bound_each_wait(kmt);
  constexpr std::uint32_t five_minutes = 30u;
  kmt.time_out_while_pulsing(five_minutes);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + five_minutes + 1u);
  CHECK(!transport.faulted());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, a_pulse_that_stops_is_given_up_on_after_its_silence)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  bound_each_wait(kmt);
  kmt.time_out_while_pulsing(2u);
  kmt.time_out_silently(64u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 2u + silent_waits);
  CHECK(transport.faulted()); }

TEST(kmt_transport, a_drain_waits_for_a_ring_a_minute_behind_its_fence_while_the_pulse_beats)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  constexpr std::uint32_t one_minute = 60000u;
  fixture.kmt().consume_late_at(1u, one_minute);
  SlowDevice device(fixture.kmt(), transport.context(), true, 0u, 0u);
  fixture.kmt().reads_clock(&device, millisecond);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  fixture.kmt().reads_clock(nullptr, 0u);
  CHECK(device.reads() >= one_minute);
  CHECK(!transport.faulted());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, a_wait_that_times_out_on_a_faulted_fence_page_gives_up_at_once)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  kmt.time_out_faulted();
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 1u);
  CHECK(transport.faulted()); }

TEST(kmt_transport, an_idle_context_reads_its_fault_and_its_code_off_the_page)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::d3d, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK(!transport.host_faulted());
  CHECK_EQ(transport.host_fault_code(), 0u);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(!transport.host_faulted());
  CHECK_EQ(transport.host_fault_code(), 0u);
  CHECK(kmt.fault_page_of(transport.context()));
  CHECK(transport.host_faulted());
  CHECK_EQ(transport.host_fault_code(), 7u);
  CHECK(!transport.faulted());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, without_a_fence_page_only_the_head_says_the_host_is_working)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  kmt.answer_views_up_to(av::ContextViewsRecord::without_fence);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(!transport.knows_gpu_progress());
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  kmt.time_out_while_consuming(1u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 2u);
  CHECK(!transport.faulted()); }

TEST(kmt_transport, a_full_ring_waits_for_the_host_and_is_refused_when_that_fails)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  for (std::uint32_t i = 0u; i < 8u; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_zeros(900u);
      CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
  }
  CHECK(kmt.calls(MockKmt::Call::wait_for_context) >= 1u);
  const std::uint32_t waited = kmt.calls(MockKmt::Call::wait_for_context);
  bool refused = false;
  kmt.fail_first(MockKmt::Call::wait_for_context, 64u, 0xC0000001u);
  for (std::uint32_t i = 0u; i < 8u && !refused; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_zeros(900u);
      refused = transport.send_async(0x10001u, writer) == TransportResult::failed;
  }
  CHECK(refused);
  CHECK(kmt.calls(MockKmt::Call::wait_for_context) > waited); }

TEST(kmt_transport, send_sync_failure_paths)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  WireWriter early = transport.request_writer();
  early.write_u32(1u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x10002u, early, reply) == TransportResult::failed);
  CHECK_STATUS(transport.initialize(), Status::ok);
  WireWriter w2 = transport.request_writer();
  w2.write_u32(2u);
  CHECK(transport.send_sync(0x10002u, w2, reply) == TransportResult::failed);
  CHECK(transport.faulted()); }

TEST(kmt_transport, a_wait_the_device_refuses_fails_the_sync)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  kmt.fail_at(MockKmt::Call::wait_for_context, 1u, MockKmt::invalid_parameter);
  WireWriter w1 = transport.request_writer();
  w1.write_u32(1u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x10002u, w1, reply) == TransportResult::failed); }

TEST(kmt_transport, teardown_leaves_the_connection_as_it_found_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint32_t context = transport.context();
  CHECK(fixture.kmt().ring_of(context) != nullptr);
  transport.teardown();
  CHECK(transport.invariants_hold());
  CHECK(fixture.kmt().ring_of(context) == nullptr);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::lock), 0u);
  CHECK_EQ(fixture.kmt().misuses(), 0u);
  CHECK_EQ(fixture.kmt().open_objects(), 3u);
  fixture.kmt().fail_first(MockKmt::Call::escape, 2u, MockKmt::invalid_parameter);
  CHECK(transport.initialize() != Status::ok);
  CHECK(transport.invariants_hold());
  CHECK_EQ(fixture.kmt().misuses(), 0u);
  CHECK_EQ(fixture.kmt().open_objects(), 3u); }

TEST(kmt_transport, the_invariant_catches_an_established_context_with_no_name)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK(transport.invariants_hold());
  KmtTransportProbe::nameless(transport);
  CHECK(!transport.invariants_hold()); }

TEST(kmt_transport, the_invariant_catches_an_established_context_with_no_buffer)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  KmtTransportProbe::orphan(transport);
  CHECK(!transport.invariants_hold()); }

TEST(kmt_transport, a_fault_loses_the_context_for_every_later_send)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(!transport.faulted());
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  WireWriter w1 = transport.request_writer();
  w1.write_u32(1u);
  CHECK(transport.send_async(0x10001u, w1) == TransportResult::failed);
  CHECK(transport.faulted());
  WireWriter w2 = transport.request_writer();
  w2.write_u32(2u);
  CHECK(transport.send_async(0x10001u, w2) == TransportResult::failed);
  WireReader reply(nullptr, 0u);
  WireWriter w3 = transport.request_writer();
  w3.write_u32(3u);
  CHECK(transport.send_sync(0x10002u, w3, reply) == TransportResult::failed); }

TEST(kmt_transport, a_faulted_wait_loses_the_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fixture.kmt().fail_at(MockKmt::Call::wait_for_context, 1u, MockKmt::invalid_parameter);
  WireWriter w1 = transport.request_writer();
  w1.write_u32(1u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x10002u, w1, reply) == TransportResult::failed);
  CHECK(transport.faulted());
  WireWriter w2 = transport.request_writer();
  w2.write_u32(2u);
  CHECK(transport.send_async(0x10001u, w2) == TransportResult::failed); }

TEST(kmt_transport_nop, passes_only_an_empty_answer_and_says_how_many_bytes_came_back)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint8_t three[3] = {1u, 2u, 3u};
  fixture.kmt().queue_answer(nullptr, 0u);
  fixture.kmt().queue_answer(three, sizeof three);
  std::uint32_t answered = 99u;
  CHECK_STATUS(transport.nop(answered), Status::ok);
  CHECK_EQ(answered, 0u);
  CHECK_EQ(fixture.kmt().commands(), 1u);
  CHECK_EQ(fixture.kmt().command_id(0u), CommandHeader::nop_id);
  CHECK_EQ(fixture.kmt().command_size(0u), static_cast<std::uint32_t>(CommandHeader::encoded_size));
  CHECK_STATUS(transport.nop(answered), Status::malformed);
  CHECK_EQ(answered, 3u);
  CHECK(!transport.faulted());
  CHECK_EQ(fixture.kmt().queued_answers(), 0u);
  fixture.kmt().fail_at(MockKmt::Call::wait_for_context, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(transport.nop(answered), Status::invalid_state);
  CHECK_EQ(answered, 0u);
  CHECK(transport.faulted());
  CHECK(transport.invariants_hold()); }

namespace {
constexpr std::uint8_t  icd_ring_order = 20u;
constexpr std::size_t   icd_command_bytes = std::size_t{256} << 10u;
constexpr std::uint32_t icd_reply_bytes = std::uint32_t{64} << 10u;

class Framer {
public:
    Framer() : bytes_{}, length_(0u) {}

    Framer &add(std::uint32_t id, std::uint32_t payload, std::uint8_t flags = 0u)
    {
        const auto size = static_cast<std::uint32_t>(CommandHeader::encoded_size + payload);
        CHECK_STATUS(CommandHeader(id, size, flags).encode(bytes_ + length_, sizeof bytes_ - length_), Status::ok);
        for (std::uint32_t i = 0u; i < payload; i++)
            bytes_[length_ + CommandHeader::encoded_size + i] = static_cast<std::uint8_t>(id + i);
        length_ += size;
        return *this;
    }
    Framer &raw(const std::uint8_t *source, std::size_t count)
    {
        std::memcpy(bytes_ + length_, source, count);
        length_ += count;
        return *this;
    }

    std::uint8_t       *bytes() { return bytes_; }
    std::size_t         length() const { return length_; }

private:
    std::uint8_t bytes_[512];
    std::size_t  length_;
};

}

TEST(kmt_transport_limits, a_256_kib_command_crosses_and_one_past_the_staging_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  static std::uint8_t staging[icd_command_bytes];
  KmtTransport transport(fixture.connection(), SubApi::vulkan, icd_ring_order, icd_reply_bytes, staging,
                         sizeof staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  AvContextRecord record;
  CHECK_STATUS(record.decode(fixture.kmt().last_private(), fixture.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.ring_order(), icd_ring_order);
  CHECK_EQ(record.reply_bytes(), icd_reply_bytes);
  std::uint8_t answer[MockKmt::max_answer] = {};
  answer[MockKmt::max_answer - 1u] = 0x5Au;
  fixture.kmt().queue_answer(answer, sizeof answer);
  WireWriter writer = transport.request_writer();
  for (std::size_t i = 0u; i < icd_command_bytes - CommandHeader::encoded_size; i++)
      writer.write_u8(static_cast<std::uint8_t>(i * 13u));
  CHECK_STATUS(writer.status(), Status::ok);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x10007u, writer, reply) == TransportResult::ok);
  CHECK_EQ(reply.remaining(), static_cast<std::size_t>(MockKmt::max_answer));
  CHECK_EQ(fixture.kmt().command_id(0u), 0x10007u);
  CHECK_EQ(fixture.kmt().command_size(0u), static_cast<std::uint32_t>(icd_command_bytes));
  CHECK_EQ(fixture.kmt().command_bytes(0u)[CommandHeader::encoded_size + 1u], 13u);
  const std::uint32_t submits = fixture.kmt().calls(MockKmt::Call::submit);
  WireWriter too_big = transport.request_writer();
  too_big.write_zeros(icd_command_bytes - CommandHeader::encoded_size + 1u);
  CHECK(transport.send_async(0x10007u, too_big) == TransportResult::failed);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), submits);
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport_limits, a_command_larger_than_the_ring_is_refused_without_a_submission)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[8192] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  WireWriter writer = transport.request_writer();
  writer.write_zeros(4096u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 0u); }

TEST(kmt_transport_limits, staging_with_no_room_for_a_payload_is_refused_at_initialize)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[CommandHeader::encoded_size + 1u] = {};
  const std::uint32_t allocations = fixture.kmt().calls(MockKmt::Call::create_allocation);
  { KmtTransport header_only(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging,
                             CommandHeader::encoded_size);
    CHECK_STATUS(header_only.initialize(), Status::invalid_parameter);
    CHECK(!header_only.established()); }
  { KmtTransport none(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, nullptr, sizeof staging);
    CHECK_STATUS(none.initialize(), Status::invalid_parameter);
    WireWriter writer = none.request_writer();
    writer.write_u8(1u);
    CHECK_STATUS(writer.status(), Status::too_large); }
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::create_allocation), allocations);
  KmtTransport smallest(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
  CHECK_STATUS(smallest.initialize(), Status::ok); }

TEST(kmt_transport_send_batch, publishes_the_framed_commands_once_and_reads_the_answer)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  Framer framed;
  framed.add(0x10001u, 4u).add(0x10002u, 8u, CommandHeader::generate_reply).add(0x10003u, 0u);
  const std::uint8_t answer[4] = {9u, 8u, 7u, 6u};
  fixture.kmt().queue_answer(answer, sizeof answer);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 1u);
  CHECK_EQ(reply.remaining(), 4u);
  CHECK_EQ(reply.read_u8(), 9u);
  const std::uint8_t *const ring = fixture.kmt().ring_of(transport.context());
  BatchHeader batch;
  CHECK_STATUS(batch.decode(ring + 32u, BatchHeader::encoded_size), Status::ok);
  CHECK_EQ(batch.fence_id(), 1u);
  CHECK_EQ(batch.length(), framed.length());
  CHECK(std::memcmp(ring + 32u + BatchHeader::encoded_size, framed.bytes(), framed.length()) == 0);
  CHECK(!transport.faulted());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport_send_batch, a_batch_asking_for_no_answer_does_not_wait)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  Framer framed;
  framed.add(0x10001u, 16u).add(0x10001u, 3u);
  const std::uint8_t leftover[2] = {1u, 2u};
  WireReader reply(leftover, sizeof leftover);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::ok);
  CHECK_EQ(reply.remaining(), 0u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 0u); }

TEST(kmt_transport_send_batch, an_answered_batch_retires_the_blob_leases_at_the_end_of_its_call)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::d3d, 12u, reply_bytes, staging, sizeof staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint8_t room[64] = {};
  transport.blobs().lend(room, sizeof room);
  CHECK(transport.blobs().take(16u).taken());
  Framer quiet;
  quiet.add(0x10001u, 4u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_batch(quiet.bytes(), quiet.length(), reply) == TransportResult::ok);
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 16u);
  Framer asking;
  asking.add(0x10001u, 4u).add(0x10002u, 0u, CommandHeader::generate_reply);
  const std::uint8_t nothing = 0u;
  fixture.kmt().queue_answer(&nothing, 0u);
  CHECK(transport.send_batch(asking.bytes(), asking.length(), reply) == TransportResult::ok);
  CHECK_EQ(transport.blobs().used(), 16u);
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 0u);
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport_send_batch, a_malformed_batch_is_refused_before_publishing)
{ struct Case { const char *what; std::uint32_t script; };
  const Case cases[] = {{"empty", 0u},         {"null", 1u},     {"cut header", 2u}, {"size past", 3u},
                        {"size below header", 4u}, {"trailing", 5u}, {"flag", 6u},     {"too large", 7u},
                        {"past 32 bits", 8u}};
  for (const Case &one : cases) {
      Fixture fixture;
      CHECK_STATUS(fixture.open(), Status::ok);
      std::uint8_t staging[staging_bytes] = {};
      KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
      CHECK_STATUS(transport.initialize(), Status::ok);
      Framer framed;
      framed.add(0x10001u, 4u, CommandHeader::generate_reply);
      const std::uint8_t *bytes = framed.bytes();
      std::size_t         length = framed.length();
      std::uint8_t        big[8192] = {};
      if (one.script == 0u) length = 0u;
      if (one.script == 1u) bytes = nullptr;
      if (one.script == 2u) length = CommandHeader::encoded_size - 1u;
      if (one.script == 3u) length -= 1u;
      if (one.script == 4u) {
          const std::uint32_t tiny = 4u;
          std::memcpy(framed.bytes() + 4u, &tiny, sizeof tiny);
      }
      if (one.script == 5u) {
          const std::uint8_t tail[3] = {1u, 2u, 3u};
          framed.raw(tail, sizeof tail);
          length = framed.length();
      }
      if (one.script == 6u) framed.bytes()[7] = 0x80u;
      if (one.script == 8u) length = std::size_t{1} << 32u;
      if (one.script == 7u) {
          CHECK_STATUS(CommandHeader(0x10001u, sizeof big, 0u).encode(big, sizeof big), Status::ok);
          bytes = big;
          length = sizeof big;
      }
      std::memset(staging, 0xEE, sizeof staging);
      WireReader reply(nullptr, 0u);
      CHECK(transport.send_batch(bytes, length, reply) == TransportResult::failed);
      CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 0u);
      CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 0u);
      const std::uint8_t *const ring = fixture.kmt().ring_of(transport.context());
      BatchHeader batch;
      CHECK(batch.decode(ring + 32u, BatchHeader::encoded_size) != Status::ok);
      CHECK(!transport.faulted());
      fixture.kmt().queue_answer(nullptr, 0u);
      std::uint32_t answered = 0u;
      CHECK_STATUS(transport.nop(answered), Status::ok);
  } }

TEST(kmt_transport_send_batch, an_unset_or_lost_context_refuses_and_a_failed_wait_loses_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
  Framer framed;
  framed.add(0x10002u, 8u, CommandHeader::generate_reply);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::failed);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fixture.kmt().fail_at(MockKmt::Call::wait_for_context, 1u, MockKmt::invalid_parameter);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::failed);
  CHECK(transport.faulted());
  const std::uint32_t submits = fixture.kmt().calls(MockKmt::Call::submit);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::failed);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), submits);
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport_send_batch, a_refused_submission_or_a_stale_answer_fails)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  Framer framed;
  framed.add(0x10002u, 8u, CommandHeader::generate_reply);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::failed);
  CHECK(transport.faulted());
  transport.teardown();
  CHECK_STATUS(transport.initialize(), Status::ok);
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::failed);
  CHECK(transport.faulted()); }

TEST(kmt_transport, a_disconnect_under_the_transport_is_recovered_from_by_the_transport)
{ Fixture fixture;
  fixture.kmt().allow_hardware_scheduling();
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint32_t first = fixture.kmt().doorbell_handle(0u);
  CHECK(first != 0u);

  WireWriter opening = transport.request_writer();
  opening.write_u32(7u);
  CHECK(transport.send_async(1u, opening) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_doorbell(), 1u);

  fixture.kmt().set_doorbell_status(first, static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry));
  WireWriter refused = transport.request_writer();
  refused.write_u32(8u);
  CHECK(transport.send_async(2u, refused) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_kernel(), 1u);
  CHECK_EQ(transport.submissions_through_doorbell(), 1u);

  WireWriter after = transport.request_writer();
  after.write_u32(9u);
  CHECK(transport.send_async(3u, after) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_doorbell(), 2u);
  CHECK_EQ(transport.submissions_through_kernel(), 1u);
  CHECK_EQ(fixture.kmt().doorbell_handle(1u), 0u);

  fixture.kmt().set_doorbell_status(first, static_cast<std::uint32_t>(DoorbellStatus::disconnected_abort));
  WireWriter aborted = transport.request_writer();
  aborted.write_u32(10u);
  CHECK(transport.send_async(4u, aborted) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_kernel(), 2u);
  const std::uint32_t second = fixture.kmt().doorbell_handle(1u);
  CHECK(second != 0u && second != first);
  WireWriter remade = transport.request_writer();
  remade.write_u32(11u);
  CHECK(transport.send_async(5u, remade) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_doorbell(), 3u);
  CHECK_EQ(transport.submissions_through_kernel(), 2u);
  CHECK_EQ(fixture.kmt().doorbell_page(second), 1u);
  transport.teardown(); }

TEST(kmt_transport, give_back_doorbell_puts_every_submission_through_the_kernel)
{ Fixture fixture;
  fixture.kmt().allow_hardware_scheduling();
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  WireWriter first = transport.request_writer();
  first.write_u32(7u);
  CHECK(transport.send_async(1u, first) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_doorbell(), 1u);

  transport.give_back_doorbell();
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_doorbell), 1u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_hw_queue), 0u);
  WireWriter second = transport.request_writer();
  second.write_u32(8u);
  CHECK(transport.send_async(2u, second) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_doorbell(), 1u);
  CHECK_EQ(transport.submissions_through_kernel(), 1u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit_to_hw_queue), 1u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 0u);
  transport.give_back_doorbell();
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_doorbell), 1u);
  transport.teardown();
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u); }

namespace {
std::size_t frame_batch(std::uint8_t *bytes, std::uint32_t id, std::uint32_t count, bool answered)
{
    constexpr std::uint32_t size = CommandHeader::encoded_size + 4u;
    for (std::uint32_t index = 0u; index < count; index++) {
        const std::uint8_t flags = answered && index + 1u == count ? CommandHeader::generate_reply : 0u;
        CHECK_STATUS(CommandHeader(id + index, size, flags).encode(bytes + size * index, CommandHeader::encoded_size),
                     Status::ok);
    }
    return size * count;
}

TransportResult send_framed(KmtTransport &transport, const std::uint8_t *framed, std::size_t length)
{
    WireReader unanswered(nullptr, 0u);
    return transport.send_batch(framed, length, unanswered);
}

}

TEST(kmt_transport, is_ready_for_a_batch_while_the_host_holds_no_more_than_the_newest)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.ready_for_batch());
  std::uint8_t framed[12u] = {};
  const std::size_t length = frame_batch(framed, 0x10001u, 1u, false);
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK(transport.ready_for_batch());
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK(!transport.ready_for_batch());
  std::uint8_t answered[12u] = {};
  const std::size_t last = frame_batch(answered, 0x10002u, 1u, true);
  fixture.kmt().queue_answer(nullptr, 0u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_batch(answered, last, reply) == TransportResult::ok);
  transport.end_call();
  CHECK(transport.ready_for_batch());
  transport.teardown(); }

namespace {
class CountingGate final : public av::PublishGate {
public:
    explicit CountingGate(const MockKmt &kmt) : kmt_(kmt), passes_(0u), sent_at_pass_(0u) {}
    void pass() override
    {
        passes_++;
        sent_at_pass_ = kmt_.commands();
    }
    std::uint32_t passes() const { return passes_; }
    std::uint32_t sent_at_pass() const { return sent_at_pass_; }

private:
    const MockKmt &kmt_;
    std::uint32_t  passes_;
    std::uint32_t  sent_at_pass_;
};

}

TEST(kmt_transport, a_held_transport_passes_the_gate_once_before_its_next_batch_reaches_the_device)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::d3d, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CountingGate gate(fixture.kmt());
  CHECK(transport.ready_for_batch());
  transport.hold(gate);
  CHECK(!transport.ready_for_batch());
  CHECK_EQ(gate.passes(), 0u);
  std::uint8_t framed[12u] = {};
  const std::size_t length = frame_batch(framed, 0x10001u, 1u, false);
  const std::uint32_t before = fixture.kmt().commands();
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK_EQ(gate.passes(), 1u);
  CHECK_EQ(gate.sent_at_pass(), before);
  CHECK_EQ(fixture.kmt().commands(), before + 1u);
  CHECK(transport.ready_for_batch());
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK_EQ(gate.passes(), 1u);
  CHECK(transport.invariants_hold());
  transport.teardown(); }

TEST(kmt_transport, an_answered_call_on_a_held_transport_passes_the_gate_before_it_is_sent)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::d3d, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CountingGate gate(fixture.kmt());
  transport.hold(gate);
  std::uint8_t answered[12u] = {};
  const std::size_t length = frame_batch(answered, 0x10002u, 1u, true);
  const std::uint8_t answer[1] = {0x42u};
  fixture.kmt().queue_answer(answer, sizeof answer);
  const std::uint32_t before = fixture.kmt().commands();
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_batch(answered, length, reply) == TransportResult::ok);
  CHECK_EQ(reply.read_u8(), 0x42u);
  transport.end_call();
  CHECK_EQ(gate.passes(), 1u);
  CHECK_EQ(gate.sent_at_pass(), before);
  transport.teardown(); }

TEST(kmt_transport, a_gate_forgotten_drops_its_own_hold_and_no_other)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::d3d, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CountingGate holding(fixture.kmt());
  CountingGate other(fixture.kmt());
  transport.hold(holding);
  transport.forget(other);
  CHECK(!transport.ready_for_batch());
  transport.forget(holding);
  CHECK(transport.ready_for_batch());
  std::uint8_t framed[12u] = {};
  const std::size_t length = frame_batch(framed, 0x10001u, 1u, false);
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK_EQ(holding.passes(), 0u);
  CHECK_EQ(other.passes(), 0u);
  transport.teardown(); }

TEST(kmt_transport, several_held_gates_are_each_passed_once)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::d3d, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CountingGate first(fixture.kmt());
  CountingGate second(fixture.kmt());
  transport.hold(first);
  transport.hold(second);
  CHECK(!transport.ready_for_batch());
  std::uint8_t framed[12u] = {};
  const std::size_t length = frame_batch(framed, 0x10001u, 1u, false);
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK_EQ(first.passes(), 1u);
  CHECK_EQ(second.passes(), 1u);
  CHECK(transport.ready_for_batch());
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK_EQ(first.passes(), 1u);
  CHECK_EQ(second.passes(), 1u);
  transport.teardown(); }

TEST(kmt_transport, a_torn_down_transport_forgets_what_held_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::d3d, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CountingGate gate(fixture.kmt());
  transport.hold(gate);
  transport.teardown();
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.ready_for_batch());
  std::uint8_t framed[12u] = {};
  const std::size_t length = frame_batch(framed, 0x10001u, 1u, false);
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK_EQ(gate.passes(), 0u);
  transport.teardown(); }

TEST(kmt_transport, the_marker_address_is_the_dma_buffer_of_a_made_transport)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_EQ(transport.marker_address(), 0u);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint64_t marker = transport.marker_address();
  CHECK(marker != 0u);
  std::uint8_t second_staging[staging_bytes] = {};
  KmtTransport second(fixture.connection(), SubApi::opengl, 12u, reply_bytes, second_staging,
                      sizeof second_staging);
  CHECK_STATUS(second.initialize(), Status::ok);
  CHECK(second.marker_address() != 0u);
  CHECK(second.marker_address() != marker);
  second.teardown();
  transport.teardown();
  CHECK_EQ(transport.marker_address(), 0u); }

namespace {
void queue_part(MockKmt &kmt, std::uint64_t length, std::uint8_t first, std::uint32_t bytes)
{
    std::uint8_t part[8u + 256u] = {};
    WireWriter   writer(part, sizeof part);
    writer.write_u64(length);
    for (std::uint32_t index = 0u; index < bytes; ++index) writer.write_u8(static_cast<std::uint8_t>(first + index));
    kmt.queue_answer(part, static_cast<std::uint32_t>(writer.offset()));
}

}

TEST(kmt_transport, a_command_longer_than_the_staging_goes_in_pieces_a_batch_each)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     transport_staging[staging_bytes] = {};
  KmtTransport     transport(fixture.connection(), memory, SubApi::vulkan, 12u, reply_bytes, transport_staging,
                             sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint32_t submitted = fixture.kmt().calls(MockKmt::Call::submit);
  WireWriter writer = transport.request_writer();
  for (std::uint32_t index = 0u; index < 2500u; ++index) writer.write_u8(static_cast<std::uint8_t>(index));
  CHECK(writer.spilled());
  CHECK(transport.send_async(0x10007u, writer) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), submitted + 3u);
  CHECK(!transport.faulted());
  CHECK_EQ(memory.live_allocations(), 0u);
  transport.teardown(); }

TEST(kmt_transport, the_command_closing_its_pieces_carries_the_rest_and_its_flags)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     transport_staging[staging_bytes] = {};
  KmtTransport     transport(fixture.connection(), memory, SubApi::vulkan, 12u, reply_bytes, transport_staging,
                             sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint8_t answer[] = {4u, 2u};
  fixture.kmt().queue_answer(answer, sizeof answer);
  WireWriter writer = transport.request_writer();
  writer.write_zeros(1100u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x10008u, writer, reply) == TransportResult::ok);
  CHECK_EQ(reply.remaining(), 2u);
  transport.end_call();
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(fixture.kmt().command_id(0u), CommandHeader::piece_id);
  CHECK_EQ(fixture.kmt().command_size(0u), staging_bytes);
  CHECK_EQ(fixture.kmt().command_id(1u), 0x10008u);
  CHECK_EQ(fixture.kmt().command_size(1u), CommandHeader::encoded_size + 1100u - (staging_bytes - 8u));
  CommandHeader header;
  CHECK_STATUS(header.decode(fixture.kmt().command_bytes(0u), CommandHeader::encoded_size), Status::ok);
  CHECK(!header.generates_reply());
  CHECK_STATUS(header.decode(fixture.kmt().command_bytes(1u), CommandHeader::encoded_size), Status::ok);
  CHECK(header.generates_reply());
  transport.teardown(); }

TEST(kmt_transport, an_answer_too_long_for_the_slot_is_fetched_in_parts)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     transport_staging[staging_bytes] = {};
  KmtTransport     transport(fixture.connection(), memory, SubApi::vulkan, 12u, reply_bytes, transport_staging,
                             sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fixture.kmt().queue_too_large(300u);
  queue_part(fixture.kmt(), 300u, 0u, 200u);
  queue_part(fixture.kmt(), 300u, 200u, 100u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x10009u, writer, reply) == TransportResult::ok);
  CHECK_EQ(reply.remaining(), 300u);
  std::uint32_t wrong = 0u;
  for (std::uint32_t index = 0u; index < 300u; ++index)
      if (reply.read_u8() != static_cast<std::uint8_t>(index)) wrong++;
  CHECK_EQ(wrong, 0u);
  CHECK_EQ(fixture.kmt().commands(), 3u);
  CHECK_EQ(fixture.kmt().command_id(0u), 0x10009u);
  for (std::uint32_t part = 1u; part < 3u; ++part) {
      CHECK_EQ(fixture.kmt().command_id(part), CommandHeader::answer_piece_id);
      WireReader asked(fixture.kmt().command_bytes(part) + CommandHeader::encoded_size, 8u);
      CHECK_EQ(asked.read_u64(), part == 1u ? 0u : 200u);
  }
  transport.end_call();
  CHECK(!transport.faulted());
  CHECK_EQ(fixture.kmt().queued_answers(), 0u);
  transport.teardown(); }

TEST(kmt_transport, a_long_answer_with_no_memory_fails_the_call_and_a_lying_host_faults)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     transport_staging[staging_bytes] = {};
  KmtTransport     transport(fixture.connection(), memory, SubApi::vulkan, 12u, reply_bytes, transport_staging,
                             sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fixture.kmt().queue_too_large(300u);
  queue_part(fixture.kmt(), 300u, 0u, 200u);
  memory.fail_allocation_at(1u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x1000Au, writer, reply) == TransportResult::failed);
  transport.end_call();
  CHECK(!transport.faulted());
  const std::uint8_t answer[] = {7u};
  fixture.kmt().queue_answer(answer, sizeof answer);
  WireWriter next = transport.request_writer();
  next.write_u32(2u);
  CHECK(transport.send_sync(0x1000Bu, next, reply) == TransportResult::ok);
  CHECK_EQ(reply.read_u8(), 7u);
  transport.end_call();
  fixture.kmt().queue_too_large(300u);
  queue_part(fixture.kmt(), 300u, 0u, 200u);
  queue_part(fixture.kmt(), 299u, 200u, 99u);
  WireWriter lied = transport.request_writer();
  lied.write_u32(3u);
  CHECK(transport.send_sync(0x1000Cu, lied, reply) == TransportResult::failed);
  transport.end_call();
  CHECK(transport.faulted());
  transport.teardown(); }

TEST(kmt_transport, without_memory_a_command_past_the_staging_fails_alone)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  WireWriter writer = transport.request_writer();
  writer.write_zeros(staging_bytes);
  CHECK(!writer.spilled());
  CHECK(transport.send_async(0x10007u, writer) == TransportResult::failed);
  CHECK(!transport.faulted());
  transport.teardown(); }

namespace {
class Holding {
public:
    Holding(Fixture &fixture, av::MockPlatform &memory, std::uint8_t ring_order, std::uint8_t *block,
            std::size_t capacity)
        : staging_{}, transport_(fixture.connection(), memory, SubApi::d3d, ring_order, reply_bytes, staging_,
                                 sizeof staging_)
    {
        CHECK_STATUS(transport_.initialize(), Status::ok);
        if (block != nullptr) transport_.hold_in(block, capacity);
    }
    ~Holding() { transport_.teardown(); }

    KmtTransport &transport() { return transport_; }
    TransportResult send(std::uint32_t id, std::uint32_t body)
    {
        WireWriter writer = transport_.request_writer();
        writer.write_zeros(body);
        return transport_.send_async(id, writer);
    }

private:
    std::uint8_t staging_[staging_bytes];
    KmtTransport transport_;
};

}

TEST(kmt_transport_holding, what_is_sent_asynchronously_is_held_until_a_flush_sends_it_whole)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  CHECK_EQ(transport.holding_capacity(), sizeof block);
  CHECK_EQ(transport.held(), 0u);
  CHECK(transport.flush() == TransportResult::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
  CHECK(holding.send(0x30002u, 4u) == TransportResult::ok);
  CHECK_EQ(transport.held(), 2u * (CommandHeader::encoded_size + 4u));
  CHECK_EQ(fixture.kmt().commands(), 0u);
  CHECK(transport.flush() == TransportResult::ok);
  CHECK_EQ(transport.held(), 0u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(fixture.kmt().command_id(1u), 0x30002u);
  CHECK(transport.invariants_hold());
  Holding plain(fixture, memory, 16u, nullptr, 0u);
  CHECK_EQ(plain.transport().holding_capacity(), 0u);
  CHECK(plain.send(0x30003u, 4u) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), 3u);
  CHECK_EQ(plain.transport().held(), 0u); }

TEST(kmt_transport_holding, an_answered_command_goes_last_in_one_batch_with_what_is_held)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
  const std::uint8_t answer[] = {0x42u};
  fixture.kmt().queue_answer(answer, sizeof answer);
  WireWriter writer = transport.request_writer();
  writer.write_u32(7u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x30002u, writer, reply) == TransportResult::ok);
  CHECK_EQ(reply.read_u8(), 0x42u);
  transport.end_call();
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(fixture.kmt().command_id(0u), 0x30001u);
  CHECK_EQ(fixture.kmt().command_id(1u), 0x30002u);
  CHECK_EQ(transport.held(), 0u); }

TEST(kmt_transport_holding, a_command_of_the_transports_own_goes_behind_what_is_held)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  std::uint8_t     room[64] = {};
  transport.blobs().lend(room, sizeof room);
  WireWriter held = transport.request_writer();
  const av::ClientBlobLease shader = transport.blobs().take(48u);
  std::memset(shader.data(), 0x5A, shader.bytes());
  held.write_u64(shader.offset());
  CHECK(transport.send_async(0x30001u, held) == TransportResult::ok);
  CHECK(!transport.blobs().take(48u).taken());
  std::uint8_t answer[av::BlobImportAnswer::encoded_size] = {};
  CHECK_STATUS(av::BlobImportAnswer(av::BlobImportAnswer::imported, 0x77u).encode(answer, sizeof answer), Status::ok);
  fixture.kmt().queue_answer(answer, sizeof answer);
  av::BlobImport heap(transport);
  CHECK_STATUS(heap.import(5u), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(fixture.kmt().command_id(0u), 0x30001u);
  CHECK_EQ(fixture.kmt().command_id(1u), CommandHeader::import_blob_id);
  CHECK_EQ(transport.blobs().used(), 0u);
  CHECK_EQ(room[0], 0x5Au);
  CHECK_EQ(transport.blobs().take(48u).offset(), 0u);
  heap.forget();
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport_holding, a_long_commands_lease_stays_lent_across_the_batches_its_pieces_fill)
{ for (const bool holds : {false, true}) {
      Fixture fixture;
      CHECK_STATUS(fixture.open(), Status::ok);
      av::MockPlatform memory;
      std::uint8_t     block[2048] = {};
      Holding          holding(fixture, memory, 12u, holds ? block : nullptr, sizeof block);
      KmtTransport    &transport = holding.transport();
      std::uint8_t     room[100] = {};
      transport.blobs().lend(room, sizeof room);
      WireWriter writer = transport.request_writer();
      const av::ClientBlobLease named = transport.blobs().take(60u);
      std::memset(named.data(), 0xA5, named.bytes());
      writer.write_u64(named.offset());
      writer.write_zeros(6000u);
      CHECK(writer.spilled());
      CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
      CHECK(fixture.kmt().calls(MockKmt::Call::wait_for_context) >= 1u);
      CHECK_EQ(transport.blobs().used(), 60u);
      CHECK(!transport.blobs().take(60u).taken());
      CHECK_EQ(room[59], 0xA5u);
      CHECK(!transport.faulted());
      CHECK(room_for(transport, 100u));
      CHECK_EQ(transport.blobs().used(), 0u);
      CHECK_EQ(last_sent_command(fixture.kmt()), 0x30001u);
      CHECK_EQ(memory.live_allocations(), 0u);
      CHECK(transport.invariants_hold());
  } }

TEST(kmt_transport_holding, a_writer_finding_no_room_left_in_the_block_sends_it_first)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[2u * staging_bytes] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  CHECK(holding.send(0x30001u, 8u) == TransportResult::ok);
  CHECK(holding.send(0x30002u, staging_bytes - CommandHeader::encoded_size) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), 0u);
  CHECK(holding.send(0x30003u, 8u) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(transport.held(), CommandHeader::encoded_size + 8u);
  std::uint8_t tiny[4] = {};
  Holding      cramped(fixture, memory, 16u, tiny, sizeof tiny);
  CHECK(cramped.send(0x30004u, 0u) == TransportResult::failed);
  WireWriter nothing_lent = cramped.transport().request_writer();
  nothing_lent.write_u8(1u);
  CHECK(nothing_lent.status() != Status::ok);
  CHECK_EQ(cramped.transport().held(), 0u);
  CHECK(!cramped.transport().faulted()); }

constexpr std::size_t spill_least = 64u * 1024u;

TEST(kmt_transport_holding, a_failed_writer_is_not_held)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  WireWriter long_one = transport.request_writer();
  long_one.write_zeros(spill_least);
  CHECK(long_one.spilled());
  memory.fail_allocation_at(1u);
  long_one.write_zeros(spill_least);
  CHECK(long_one.status() != Status::ok);
  CHECK(transport.send_async(0x30001u, long_one) == TransportResult::failed);
  CHECK_EQ(memory.live_allocations(), 0u);
  WireWriter asking = transport.request_writer();
  asking.write_zeros(spill_least);
  memory.fail_allocation_at(1u);
  asking.write_zeros(spill_least);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x30002u, asking, reply) == TransportResult::failed);
  CHECK_EQ(transport.held(), 0u);
  CHECK_EQ(fixture.kmt().commands(), 0u);
  CHECK(!transport.faulted()); }

TEST(kmt_transport_holding, a_held_batch_that_cannot_be_published_loses_the_context)
{ { Fixture fixture;
    CHECK_STATUS(fixture.open(), Status::ok);
    av::MockPlatform memory;
    std::uint8_t     block[4096] = {};
    Holding          holding(fixture, memory, 16u, block, sizeof block);
    CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
    fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
    CHECK(holding.transport().flush() == TransportResult::failed);
    CHECK(holding.transport().faulted());
    CHECK_EQ(holding.transport().held(), 0u);
    CHECK(holding.transport().flush() == TransportResult::failed);
    CHECK(holding.send(0x30002u, 4u) == TransportResult::failed); }
  { Fixture fixture;
    CHECK_STATUS(fixture.open(), Status::ok);
    av::MockPlatform memory;
    static std::uint8_t block[8192];
    Holding             holding(fixture, memory, 12u, block, sizeof block);
    for (std::uint32_t command = 0u; command < 6u; command++)
        CHECK(holding.send(0x30001u, staging_bytes - CommandHeader::encoded_size) == TransportResult::ok);
    CHECK(holding.transport().flush() == TransportResult::failed);
    CHECK(holding.transport().faulted()); }
  { Fixture fixture;
    CHECK_STATUS(fixture.open(), Status::ok);
    av::MockPlatform memory;
    std::uint8_t     block[4096] = {};
    Holding          holding(fixture, memory, 16u, block, sizeof block);
    CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
    fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
    WireWriter writer = holding.transport().request_writer();
    WireReader reply(nullptr, 0u);
    CHECK(holding.transport().send_sync(0x30002u, writer, reply) == TransportResult::failed);
    CHECK(holding.transport().faulted());
    holding.transport().end_call(); }
  Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[staging_bytes] = {};
  std::uint8_t block[256] = {};
  KmtTransport unmade(fixture.connection(), SubApi::d3d, 16u, reply_bytes, staging, sizeof staging);
  unmade.hold_in(block, sizeof block);
  CHECK(unmade.flush() == TransportResult::failed);
  WireWriter writer = unmade.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(unmade.send_sync(0x30002u, writer, reply) == TransportResult::failed);
  CHECK(!unmade.faulted()); }

TEST(kmt_transport_holding, a_published_batch_ends_in_the_channel_stats_report_once_the_arena_changed)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  std::uint8_t     room[64] = {};
  transport.blobs().lend(room, sizeof room);
  CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
  CHECK(transport.flush() == TransportResult::ok);
  const std::uint32_t last = fixture.kmt().commands() - 1u;
  CHECK_EQ(fixture.kmt().command_id(last - 1u), 0x30001u);
  CHECK_EQ(fixture.kmt().command_id(last), CommandHeader::channel_stats_id);
  CHECK_EQ(fixture.kmt().command_size(last), static_cast<std::uint32_t>(av::ChannelStats::command_bytes));
  const std::uint32_t reported = fixture.kmt().commands();
  CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
  CHECK(transport.flush() == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), reported + 1u);
  CHECK_EQ(last_sent_command(fixture.kmt()), 0x30001u);
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport_holding, a_long_command_is_held_in_pieces_and_fails_where_they_cannot_go)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[2u * staging_bytes] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  WireWriter long_one = transport.request_writer();
  long_one.write_zeros(5u * staging_bytes);
  CHECK(transport.send_async(0x30001u, long_one) == TransportResult::ok);
  CHECK(fixture.kmt().commands() >= 2u);
  for (std::uint32_t index = 0u; index < fixture.kmt().commands(); index++)
      CHECK_EQ(fixture.kmt().command_id(index), CommandHeader::piece_id);
  CHECK(transport.flush() == TransportResult::ok);
  CHECK_EQ(last_sent_command(fixture.kmt()), 0x30001u);
  CHECK_EQ(memory.live_allocations(), 0u);
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  WireWriter refused = transport.request_writer();
  refused.write_zeros(5u * staging_bytes);
  CHECK(transport.send_async(0x30002u, refused) == TransportResult::failed);
  CHECK(transport.faulted());
  CHECK_EQ(memory.live_allocations(), 0u);
  std::uint8_t tiny[CommandHeader::encoded_size] = {};
  Holding      cramped(fixture, memory, 16u, tiny, sizeof tiny);
  cramped.transport().hold_in(block, sizeof block);
  WireWriter pieced = cramped.transport().request_writer();
  pieced.write_zeros(2u * staging_bytes);
  cramped.transport().hold_in(tiny, sizeof tiny);
  CHECK(cramped.transport().send_async(0x30003u, pieced) == TransportResult::failed);
  CHECK(!cramped.transport().faulted());
  CHECK_EQ(memory.live_allocations(), 0u); }

TEST(kmt_transport_holding, framed_commands_join_what_is_held)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[64] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  std::uint8_t framed[5u * 12u] = {};
  const std::size_t two = frame_batch(framed, 0x10001u, 2u, false);
  CHECK(transport.hold_framed(framed, two) == TransportResult::ok);
  CHECK_EQ(transport.held(), two);
  CHECK(transport.hold_framed(framed, two) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), 0u);
  CHECK(transport.hold_framed(framed, two) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), 4u);
  CHECK_EQ(transport.held(), two);
  const std::size_t five = frame_batch(framed, 0x10001u, 5u, false);
  CHECK(five <= sizeof block);
  CHECK(transport.hold_framed(framed, five) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), 6u);
  CHECK(transport.hold_framed(framed, 0u) == TransportResult::failed);
  CHECK(transport.hold_framed(nullptr, two) == TransportResult::failed);
  static std::uint8_t longer[128];
  CHECK(transport.hold_framed(longer, sizeof longer) == TransportResult::failed);
  CHECK_EQ(transport.held(), five);
  Holding plain(fixture, memory, 16u, nullptr, 0u);
  CHECK(plain.transport().hold_framed(framed, two) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().commands(), 8u);
  CHECK(plain.transport().hold_framed(framed, std::size_t{1} << 32u) == TransportResult::failed);
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(transport.hold_framed(framed, five) == TransportResult::failed);
  CHECK(transport.hold_framed(framed, two) == TransportResult::failed); }

TEST(kmt_transport_holding, a_framed_batch_goes_behind_what_is_held)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
  std::uint8_t framed[12u] = {};
  const std::size_t length = frame_batch(framed, 0x10001u, 1u, false);
  CHECK(send_framed(transport, framed, length) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().command_id(0u), 0x30001u);
  CHECK_EQ(fixture.kmt().command_id(1u), 0x10001u);
  CHECK(holding.send(0x30001u, 4u) == TransportResult::ok);
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(send_framed(transport, framed, length) == TransportResult::failed);
  CHECK_EQ(fixture.kmt().commands(), 2u); }

TEST(kmt_transport_holding, room_held_commands_hold_comes_back_once_they_are_sent_and_run)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  std::uint8_t     room[64] = {};
  transport.blobs().lend(room, sizeof room);
  WireWriter writer = transport.request_writer();
  writer.write_u64(transport.blobs().take(64u).offset());
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  CHECK(room_for(transport, 0u));
  CHECK_EQ(fixture.kmt().commands(), 0u);
  CHECK(room_for(transport, 64u));
  CHECK_EQ(sent_commands(fixture.kmt()), 1u);
  CHECK_EQ(transport.blobs().room(), 64u);
  WireWriter again = transport.request_writer();
  again.write_u64(transport.blobs().take(64u).offset());
  CHECK(transport.send_async(0x30001u, again) == TransportResult::ok);
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(!room_for(transport, 1u));
  CHECK(transport.faulted()); }

TEST(kmt_transport_holding, the_publish_barrier_runs_what_is_held_before_it_returns)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[4096] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  KmtTransport    &transport = holding.transport();
  std::uint8_t     room[64] = {};
  transport.blobs().lend(room, sizeof room);
  WireWriter writer = transport.request_writer();
  writer.write_u64(transport.blobs().take(32u).offset());
  CHECK(transport.send_async(0x30001u, writer) == TransportResult::ok);
  fixture.kmt().queue_answer(nullptr, 0u);
  CHECK_STATUS(transport.run_held(), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(fixture.kmt().command_id(0u), 0x30001u);
  CHECK_EQ(fixture.kmt().command_id(1u), CommandHeader::nop_id);
  CHECK_EQ(transport.blobs().used(), 0u);
  const std::uint8_t chatty[] = {1u, 2u};
  fixture.kmt().queue_answer(chatty, sizeof chatty);
  CHECK_STATUS(transport.run_held(), Status::ok);
  fixture.kmt().withhold_answer_at(1u);
  CHECK_STATUS(transport.run_held(), Status::invalid_state);
  CHECK(transport.faulted()); }

TEST(kmt_transport_holding, the_invariant_catches_more_held_than_the_block_holds)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  av::MockPlatform memory;
  std::uint8_t     block[256] = {};
  Holding          holding(fixture, memory, 16u, block, sizeof block);
  CHECK(holding.transport().invariants_hold());
  KmtTransportProbe::overhold(holding.transport());
  CHECK(!holding.transport().invariants_hold());
  holding.transport().hold_in(block, sizeof block);
  CHECK(holding.transport().invariants_hold()); }
