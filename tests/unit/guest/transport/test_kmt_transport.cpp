/* One sub-API context's transport, over MockKmt. Setting it up creates and maps
 * the ring, reply and DMA allocations and a virtual context carrying the record
 * as private data; a command sent async lands a batch in the ring and a
 * submission on the context; a synchronous command reads back the answer the
 * device wrote to the reply slot (the test plays the device). */
#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "guest/transport/publish_gate.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/batch_header.hpp"
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
    /* An established context with its DMA buffer gone: what it never builds. */
    static void orphan(KmtTransport &t) { t.context_ = 1u; t.dma_address_ = 0u; }
    /* An established context with its buffer, and no name. */
    static void nameless(KmtTransport &t) { t.context_ = 1u; t.dma_address_ = 0x1000u; t.allocations_[0] = 0u; }
};

}  // namespace av

using av::KmtTransportProbe;

namespace {

/* The av suite's transport: 1 KiB commands and a page of reply (feature 044). */
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;

const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);


/* The adapter the connection is opened on: found by the caller, as the suite
 * finds av among every adapter D3DKMT enumerates. */
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

}  // namespace

/* Feature 049: where the transport submits. On a machine whose OS hands out no
 * hardware queue - the default, and what Windows is until hardware-accelerated
 * GPU scheduling is turned on - every submission goes through the kernel and
 * the transport behaves exactly as it did before. Given a doorbell, the same
 * batches go through it and the kernel hears nothing. */
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
  /* The doorbell went with the context it was made for. */
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_doorbell), 1u); }

/* Another thread of the process waits for a batch through a mark: on the
 * hardware queue's progress fence where there is a queue, on the context where
 * there is none - and not at all before the first batch. */
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

/* The D3DKMT handle of the virtual context, which a process names to the driver
 * beside the transport - an escape binding a chunk to it (feature 045) - and
 * nothing before it exists or after it is gone. */
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
  /* The handle it answered was that context's: gone with it. */
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
  /* The record was carried as private data. */
  CHECK_EQ(fixture.kmt().last_private_size(), AvContextRecord::encoded_size);
  /* Two allocations: the page the context is named by, and the DMA buffer a
   * submission names. The ring and reply slot are the driver's. */
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::name_allocation(transport)), 17u);
  CHECK_EQ(fixture.kmt().allocation_pages(KmtTransportProbe::name_allocation(transport)), 1u);
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::dma_allocation(transport)), 16u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::lock), 0u);
  AvContextRecord record;
  CHECK_STATUS(record.decode(fixture.kmt().last_private(), fixture.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.ring(), KmtTransportProbe::name_allocation(transport));
  CHECK_EQ(record.ring_order(), 12u);
  CHECK_EQ(record.reply_bytes(), reply_bytes);
  /* The ring it writes is the one the driver made: its header is there. */
  const std::uint8_t *const ring = fixture.kmt().ring_of(transport.context());
  CHECK(ring != nullptr);
  av::RingHeader header;
  CHECK_STATUS(header.decode(ring, av::RingHeader::encoded_size), Status::ok);
  CHECK_EQ(header.order(), 12u);
  /* A second initialize is refused. */
  CHECK_STATUS(transport.initialize(), Status::invalid_state);
  CHECK(transport.invariants_hold());
  transport.teardown();
  CHECK(!transport.established()); }

/* An OpenGL context's record carries what it is made with (feature 050): the
 * options reach the private data unchanged, the ring a sharing context names
 * is this transport's own ring allocation, and nothing is answered before the
 * ring exists or after it is gone. Options the record cannot carry - on
 * another sub-API - are refused before anything is allocated. */
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

/* A command sent async is framed into a batch in the ring and submitted. */
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
  /* The ring holds a batch of fence 1 whose command is the one sent. */
  const std::uint8_t *ring = fixture.kmt().ring_of(transport.context());
  CHECK(ring != nullptr);
  BatchHeader batch;
  CHECK_STATUS(batch.decode(ring + 32u, BatchHeader::encoded_size), Status::ok);
  CHECK_EQ(batch.fence_id(), 1u);
  CommandHeader command;
  CHECK_STATUS(command.decode(ring + 32u + BatchHeader::encoded_size, CommandHeader::encoded_size), Status::ok);
  CHECK_EQ(command.id(), 0x10001u);
  /* The submission carried the descriptor of that batch as its private data. */
  CHECK_EQ(fixture.kmt().last_submission_size(), av::SubmissionDescriptor::encoded_size);
  av::SubmissionDescriptor submitted;
  CHECK_STATUS(submitted.decode(fixture.kmt().last_submission(), av::SubmissionDescriptor::encoded_size), Status::ok);
  CHECK_EQ(submitted.fence_id(), 1u);
  CHECK_EQ(submitted.begin(), 0u);
  CHECK_EQ(submitted.end(), 48u);   /* the batch, its command and 4 bytes, padded to 8 */ }

/* A writer that overflowed its slot is refused, not sent. */
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

/* A synchronous command reads back the answer the device wrote for its fence. */
TEST(kmt_transport, a_sync_command_reads_the_reply_the_device_wrote)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  /* Play the device: the first sync command takes fence 1, so write its answer
   * into the reply slot before the call reads it. */
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
  /* Ending the call releases nothing: one thread owns this transport, and the
   * reply stays where it was until the next call. */
  transport.end_call();
  CHECK_EQ(got.read_u32(), 2u);
  CHECK_EQ(fixture.kmt().commands(), 1u); }

/* Leases retire at the end of a call the host answered: it has run every
 * command sent before, one-way ones included. A one-way send, a failed call,
 * or an end with no call keep them, since the host may not have read them yet. */
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
  /* Still readable while the call's reply is decoded. */
  CHECK_EQ(transport.blobs().used(), 16u);
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 0u);
  /* One answer retires once: leases taken after it wait for the next. */
  CHECK(transport.blobs().take(8u).taken());
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 8u);
  /* A call that failed has shown nothing about what the host read. */
  fixture.kmt().withhold_answer_at(1u);
  WireWriter lost = transport.request_writer();
  lost.write_u32(3u);
  CHECK(transport.send_sync(0x30002u, lost, got) == TransportResult::failed);
  transport.end_call();
  CHECK_EQ(transport.blobs().used(), 8u);
  CHECK(transport.invariants_hold()); }

/* Room for a lease comes back from the oldest batches the host has run, one
 * wait at a time, with the newer ones left lent; room only unsent commands
 * hold is not found by waiting. */
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
  CHECK(transport.make_room_for_blobs(16u));   /* there already: no wait */
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 0u);
  CHECK(transport.make_room_for_blobs(24u));   /* the host runs what was sent */
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 1u);
  CHECK(transport.blobs().room() >= 24u);
  /* A lease taken but not yet sent holds its room until it is. */
  CHECK(transport.blobs().take(transport.blobs().room()).taken());
  CHECK(!transport.make_room_for_blobs(1u));
  CHECK(!transport.faulted());
  CHECK(transport.invariants_hold()); }

/* A host that stops while the room is waited for loses the context. */
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
  CHECK(!transport.make_room_for_blobs(8u));
  CHECK(transport.faulted());
  CHECK(!transport.make_room_for_blobs(8u)); }

/* Sending before setup is refused; the decode arena and blob arena refuse. */
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

/* An open connection and a transport over it, for the failure paths. */
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

}  // namespace

TEST(kmt_transport, a_failure_at_each_setup_step_leaves_it_unestablished)
{ refuse_setup(MockKmt::Call::create_allocation, 1u);   /* the name */
  refuse_setup(MockKmt::Call::create_allocation, 2u);   /* dma */
  refuse_setup(MockKmt::Call::make_resident, 1u);
  refuse_setup(MockKmt::Call::wait_for_fence, 1u);      /* the name's residency */
  refuse_setup(MockKmt::Call::map_gpu_virtual_address, 1u);
  refuse_setup(MockKmt::Call::create_context_virtual, 1u);
  refuse_setup(MockKmt::Call::escape, 1u); }            /* where the ring and reply slot are */

/* The last residency wait a setup makes is the DMA buffer's mapping wait; a
 * device that never reaches it fails the setup. Counting the waits keeps this
 * robust as the allocation count changes. */
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

/* Teardown leaves nothing bound: the same transport sets up a new context,
 * whose first fence is 1 again and whose ring starts empty, and a transport
 * whose setup failed after binding its ring can try again. */
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
  CHECK_EQ(fixture.kmt().open_objects(), 3u);   /* the adapter, device and paging queue */
  CHECK_EQ(fixture.kmt().misuses(), 0u); }

/* Views no driver could have answered - an address missing, or the ring and
 * reply slot at one place - set nothing up, and leave nothing open. */
TEST(kmt_transport, views_the_driver_could_not_have_answered_are_refused)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  const std::uint64_t answers[][2] = {{0u, 0x2000u}, {0x1000u, 0u}, {0x1000u, 0x1000u}};
  for (const auto &views : answers) {
      std::uint8_t answer[av::ContextViewsRecord::encoded_size] = {};
      CHECK_STATUS(av::ContextViewsRecord().answered(views[0], views[1]).encode(answer, sizeof answer), Status::ok);
      kmt.answer_next_escape(answer, sizeof answer);
      std::uint8_t transport_staging[staging_bytes] = {};
      KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging,
                             sizeof transport_staging);
      CHECK_STATUS(transport.initialize(), Status::malformed);
      CHECK(!transport.established());
      CHECK_EQ(kmt.open_objects(), 3u);
  } }

/* A submission the device refuses fails the send. */
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

/* With every fence in flight the next send waits for the host to catch up
 * rather than failing, and fails only when that wait does. */
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
  /* The context is faulted: nothing more goes. */
  WireWriter after = transport.request_writer();
  after.write_u32(65u);
  CHECK(transport.send_async(0x10001u, after) == TransportResult::failed); }

namespace {

/* A transport whose fence window is full: its next send has to wait. */
void fill_the_fence_window(KmtTransport &transport)
{
    for (std::uint32_t i = 0u; i < 64u; i++) {
        WireWriter writer = transport.request_writer();
        writer.write_u32(i);
        CHECK(transport.send_async(0x10001u, writer) == TransportResult::ok);
    }
}

}  // namespace

/* A wait that runs out its bound while the host took batches is a host that is
 * behind, not gone: it is waited again, and the send goes. */
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

/* A wait that runs out its bound with nothing taken is a host that is gone:
 * the context is faulted after that one wait, not waited on forever. */
TEST(kmt_transport, a_wait_that_times_out_with_nothing_taken_gives_up)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  kmt.fail_first(MockKmt::Call::wait_for_context, 64u, MockKmt::io_timeout);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 1u); }

/* The host took what it had, then stopped: the second wait finds the head
 * where the first left it, and that is the end. */
TEST(kmt_transport, a_host_that_stops_taking_batches_is_given_up_on_after_one_silent_wait)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  fill_the_fence_window(transport);
  const std::uint32_t before = kmt.calls(MockKmt::Call::wait_for_context);
  kmt.time_out_while_consuming(64u);
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), before + 2u); }

/* A batch with no room in the ring waits for the host to take one, and is
 * refused when that wait fails. */
TEST(kmt_transport, a_full_ring_waits_for_the_host_and_is_refused_when_that_fails)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  /* A few 900-byte commands fill an order-12 ring; the next waits it out. */
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

/* send_sync fails a call that could not be published, a device that will not
 * wait, and an answer the device never wrote. */
TEST(kmt_transport, send_sync_failure_paths)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  /* Before setup: nothing to publish. */
  WireWriter early = transport.request_writer();
  early.write_u32(1u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(0x10002u, early, reply) == TransportResult::failed);
  CHECK_STATUS(transport.initialize(), Status::ok);
  /* The wait succeeds but the reply slot holds no answer for the fence. */
  WireWriter w2 = transport.request_writer();
  w2.write_u32(2u);
  CHECK(transport.send_sync(0x10002u, w2, reply) == TransportResult::failed);
  CHECK(transport.faulted()); }

/* A device that will not answer the wait fails the call. */
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

/* Teardown destroys the context, which takes the driver's views of its ring
 * and reply slot with it, and the two allocations, leaving the connection as it
 * found it; a setup that failed after the context was made does the same. */
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
  /* The adapter, the device and the queue. */
  CHECK_EQ(fixture.kmt().open_objects(), 3u);
  fixture.kmt().fail_at(MockKmt::Call::escape, 1u, MockKmt::invalid_parameter);
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

/* A device-side failure loses the context: every later send fails, even a call
 * the device would now accept. */
TEST(kmt_transport, a_fault_loses_the_context_for_every_later_send)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(!transport.faulted());
  /* The device refuses the submission: the context is lost. */
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  WireWriter w1 = transport.request_writer();
  w1.write_u32(1u);
  CHECK(transport.send_async(0x10001u, w1) == TransportResult::failed);
  CHECK(transport.faulted());
  /* Later sends fail without even reaching the device. */
  WireWriter w2 = transport.request_writer();
  w2.write_u32(2u);
  CHECK(transport.send_async(0x10001u, w2) == TransportResult::failed);
  WireReader reply(nullptr, 0u);
  WireWriter w3 = transport.request_writer();
  w3.write_u32(3u);
  CHECK(transport.send_sync(0x10002u, w3, reply) == TransportResult::failed); }

/* A faulted wait loses the context the same way. */
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

/* The transport's own no-op: one header-only command with the reply flag,
 * judged by the answer being empty. The device answering bytes is some other
 * command's answer, and a context that cannot send is lost, not malformed. */
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

/* An ICD's transport (feature 045): an order-20 ring, 256 KiB commands and a
 * 64 KiB reply slot. */
constexpr std::uint8_t  icd_ring_order = 20u;
constexpr std::size_t   icd_command_bytes = std::size_t{256} << 10u;
constexpr std::uint32_t icd_reply_bytes = std::uint32_t{64} << 10u;

/* Commands framed one after another into `bytes`, as a caller batching its own
 * calls frames them. */
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

}  // namespace

/* The limits come from the caller: a command of the whole 256 KiB staging
 * crosses an order-20 ring into a 64 KiB reply slot, and one byte more does not
 * leave the process. */
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

/* A command the caller's staging holds but the ring cannot: refused unsent. */
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

/* Staging with no room past a command header is refused before any allocation. */
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

/* Framed commands go out as one batch: one submission whose ring bytes are the
 * caller's frames, and the answer of the command that asked for one. */
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

/* A batch no command of which asks for an answer is published without a wait,
 * and its reply is empty. */
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

/* A batch the host answered has shown it read every lease taken before it, as
 * a synchronous command does: ending the call retires them. One asking for no
 * answer shows nothing, and ending after it keeps them. Without this the
 * Direct3D stream's arena, which only ever answers through batches, filled
 * once and refused every large upload after. */
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

/* A batch that does not frame exactly is refused before anything is written or
 * submitted, and the context is not lost for it. */
TEST(kmt_transport_send_batch, a_malformed_batch_is_refused_before_publishing)
{ struct Case { const char *what; std::uint32_t script; };
  /* 0 no bytes, 1 null, 2 a header cut short, 3 a size past the batch, 4 a size
   * below the header, 5 trailing bytes, 6 an unknown flag, 7 too large a batch
   * for the ring, 8 a length no ring counts */
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
      if (one.script == 8u) length = std::size_t{1} << 32u;   /* judged by its length, never read */
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
      CHECK(batch.decode(ring + 32u, BatchHeader::encoded_size) != Status::ok);   /* nothing was written */
      CHECK(!transport.faulted());
      fixture.kmt().queue_answer(nullptr, 0u);
      std::uint32_t answered = 0u;
      CHECK_STATUS(transport.nop(answered), Status::ok);
  } }

/* A batch on a context not set up, or lost, is refused unsent; a replying
 * batch whose wait fails loses the context, as a synchronous send does. */
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

/* A submission the driver refuses loses the context, and an answer that is not
 * for the batch's fence fails the call. */
TEST(kmt_transport_send_batch, a_refused_submission_or_a_stale_answer_fails)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u, reply_bytes, staging, sizeof staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  Framer framed;
  framed.add(0x10002u, 8u, CommandHeader::generate_reply);
  WireReader reply(nullptr, 0u);
  /* The wait succeeds, but nothing was written to the reply slot for this fence. */
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::failed);
  CHECK(transport.faulted());
  transport.teardown();
  CHECK_STATUS(transport.initialize(), Status::ok);
  fixture.kmt().fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  CHECK(transport.send_batch(framed.bytes(), framed.length(), reply) == TransportResult::failed);
  CHECK(transport.faulted()); }

/* US2 at the transport: dxgkrnl disconnects the doorbell between two
 * submissions, and nothing above the transport hears of it. A retry is
 * connected again and a submission later the doorbell path is back; an abort
 * costs a doorbell, and the transport makes another rather than staying on the
 * kernel path forever. */
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

  /* Retry, and the submission that finds it. */
  fixture.kmt().set_doorbell_status(first, static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry));
  WireWriter refused = transport.request_writer();
  refused.write_u32(8u);
  CHECK(transport.send_async(2u, refused) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_kernel(), 1u);
  CHECK_EQ(transport.submissions_through_doorbell(), 1u);

  /* Nothing told it to reconnect: it did that when it was refused, so the next
   * submission is through the doorbell again. */
  WireWriter after = transport.request_writer();
  after.write_u32(9u);
  CHECK(transport.send_async(3u, after) == av::TransportResult::ok);
  CHECK_EQ(transport.submissions_through_doorbell(), 2u);
  CHECK_EQ(transport.submissions_through_kernel(), 1u);
  CHECK_EQ(fixture.kmt().doorbell_handle(1u), 0u);

  /* Abort: one submission through the kernel, a second doorbell made, and the
   * one after it through that. */
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

/* A process that gives its doorbell back submits through the kernel from then
 * on: what the in-guest benchmark does to measure both paths on one boot
 * (feature 049, US3). */
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

  /* The doorbell goes back and the queue stays: a hardware-scheduled context
   * submits through its queue and through nothing else (research.md §13), so
   * that is what the kernel path is for it. */
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
  /* Given back twice is no second doorbell to give, and the teardown is what
   * gives the queue back. */
  transport.give_back_doorbell();
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_doorbell), 1u);
  transport.teardown();
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u); }

namespace {

/* `count` framed commands of four bytes of body each, numbered from `id`; the
 * last asks for an answer when `answered`. */
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

}  // namespace

/* A stream that framed its batch itself and knows none of it answers sends it
 * whole, waits for nothing, and the device finds every command in it. */
TEST(kmt_transport, an_async_batch_is_sent_whole_without_a_wait)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  std::uint8_t framed[3u * 12u] = {};
  const std::size_t length = frame_batch(framed, 0x10001u, 3u, false);
  /* Nothing is sent before the transport is made. */
  CHECK(transport.send_async_batch(framed, length) == TransportResult::failed);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.send_async_batch(nullptr, length) == TransportResult::failed);
  CHECK(transport.send_async_batch(framed, 0u) == TransportResult::failed);
  CHECK(transport.send_async_batch(framed, std::size_t{1} << 32u) == TransportResult::failed);
  CHECK_EQ(fixture.kmt().commands(), 0u);
  CHECK(transport.send_async_batch(framed, length) == TransportResult::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 0u);
  CHECK_EQ(fixture.kmt().commands(), 3u);
  CHECK_EQ(fixture.kmt().command_id(0u), 0x10001u);
  CHECK_EQ(fixture.kmt().command_id(2u), 0x10003u);
  CHECK(transport.invariants_hold());
  transport.teardown(); }

/* A batch ending in the one command that answers is awaited, and the reply is
 * that command's. */
TEST(kmt_transport, an_answered_batch_is_awaited_and_answers_its_last_command)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::opengl, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  std::uint8_t framed[2u * 12u] = {};
  const std::size_t length = frame_batch(framed, 0x20001u, 2u, true);
  WireReader reply(framed, sizeof framed);
  CHECK(transport.send_answered_batch(framed, length, reply) == TransportResult::failed);
  CHECK_EQ(reply.remaining(), 0u);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.send_answered_batch(nullptr, length, reply) == TransportResult::failed);
  CHECK(transport.send_answered_batch(framed, 0u, reply) == TransportResult::failed);
  CHECK(transport.send_answered_batch(framed, std::size_t{1} << 32u, reply) == TransportResult::failed);
  const std::uint8_t answer[] = {0x42u, 0x43u};
  fixture.kmt().queue_answer(answer, sizeof answer);
  CHECK(transport.send_answered_batch(framed, length, reply) == TransportResult::ok);
  CHECK_EQ(reply.remaining(), 2u);
  CHECK_EQ(reply.read_u8(), 0x42u);
  transport.end_call();
  CHECK_EQ(fixture.kmt().commands(), 2u);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), 1u);
  CHECK_EQ(fixture.kmt().queued_answers(), 0u);
  /* A wait that fails faults the transport: nothing more is sent. */
  fixture.kmt().fail_at(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  CHECK(transport.send_answered_batch(framed, length, reply) == TransportResult::failed);
  CHECK(transport.send_async_batch(framed, length) == TransportResult::failed);
  transport.teardown(); }

/* The host has taken every batch but the newest: one more now keeps it fed. A
 * second batch it has not begun is the one to hold back and grow. */
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
  CHECK(transport.send_async_batch(framed, length) == TransportResult::ok);
  CHECK(transport.ready_for_batch());
  CHECK(transport.send_async_batch(framed, length) == TransportResult::ok);
  CHECK(!transport.ready_for_batch());
  /* The device finishing what was sent - a wait plays it - moves the head. */
  std::uint8_t answered[12u] = {};
  const std::size_t last = frame_batch(answered, 0x10002u, 1u, true);
  fixture.kmt().queue_answer(nullptr, 0u);
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_answered_batch(answered, last, reply) == TransportResult::ok);
  transport.end_call();
  CHECK(transport.ready_for_batch());
  transport.teardown(); }

namespace {

/* Counts its passes and what the device had been sent at each: a gate passed
 * after its batch was submitted would have let the host start it too soon. */
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

}  // namespace

/* After a present the transport is held: no batch is ready however idle the
 * host, so hints keep growing theirs, and the one that is sent passes the gate
 * before the device hears of it - once, the batches after it go as before. */
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
  CHECK(transport.send_async_batch(framed, length) == TransportResult::ok);
  CHECK_EQ(gate.passes(), 1u);
  CHECK_EQ(gate.sent_at_pass(), before);
  CHECK_EQ(fixture.kmt().commands(), before + 1u);
  CHECK(transport.ready_for_batch());
  CHECK(transport.send_async_batch(framed, length) == TransportResult::ok);
  CHECK_EQ(gate.passes(), 1u);
  CHECK(transport.invariants_hold());
  transport.teardown(); }

/* An answered call sends what is held as surely as a flush does: the gate is
 * passed before its batch, and the answer read after. */
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
  CHECK(transport.send_answered_batch(answered, length, reply) == TransportResult::ok);
  CHECK_EQ(reply.read_u8(), 0x42u);
  transport.end_call();
  CHECK_EQ(gate.passes(), 1u);
  CHECK_EQ(gate.sent_at_pass(), before);
  transport.teardown(); }

/* A device going away takes its own hold with it and leaves another's: the
 * transport is its session's, shared by every device in it. */
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
  CHECK(transport.send_async_batch(framed, length) == TransportResult::ok);
  CHECK_EQ(holding.passes(), 0u);
  CHECK_EQ(other.passes(), 0u);
  transport.teardown(); }

/* A gate is its device's: a transport torn down and set up again holds nothing. */
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
  CHECK(transport.send_async_batch(framed, length) == TransportResult::ok);
  CHECK_EQ(gate.passes(), 0u);
  transport.teardown(); }

/* What a packet on another of this process's contexts names: the transport's
 * DMA buffer, which exists only while the context does. */
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
  /* A second transport's buffer is its own. */
  std::uint8_t second_staging[staging_bytes] = {};
  KmtTransport second(fixture.connection(), SubApi::opengl, 12u, reply_bytes, second_staging,
                      sizeof second_staging);
  CHECK_STATUS(second.initialize(), Status::ok);
  CHECK(second.marker_address() != 0u);
  CHECK(second.marker_address() != marker);
  second.teardown();
  transport.teardown();
  CHECK_EQ(transport.marker_address(), 0u); }
