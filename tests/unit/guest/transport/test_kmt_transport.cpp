/* One sub-API context's transport, over MockKmt. Setting it up creates and maps
 * the ring, reply and DMA allocations and a virtual context carrying the record
 * as private data; a command sent async lands a batch in the ring and a
 * submission on the context; a synchronous command reads back the answer the
 * device wrote to the reply slot (the test plays the device). */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BatchHeader;
using av::CommandHeader;
using av::GuestConnection;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::ReplyHeader;
using av::RvkContextRecord;
using av::Status;
using av::SubApi;
using av::SubmissionDescriptor;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;

namespace av {

struct KmtTransportProbe {
    static std::uint32_t ring_allocation(const KmtTransport &t) { return t.allocations_[0]; }
    static std::uint32_t fence_allocation(const KmtTransport &t) { return t.allocations_[1]; }
    static std::uint32_t reply_allocation(const KmtTransport &t) { return t.allocations_[2]; }
    static std::uint32_t dma_allocation(const KmtTransport &t) { return t.allocations_[3]; }
    /* An established context with its DMA buffer gone: what it never builds. */
    static void orphan(KmtTransport &t) { t.context_ = 1u; t.dma_address_ = 0u; }
    /* A view held of an allocation that does not exist. */
    static void phantom_view(KmtTransport &t) { t.locked_[2] = true; t.allocations_[2] = 0u; }
};

}  // namespace av

using av::KmtTransportProbe;

namespace {

/* The rvk suite's transport: 1 KiB commands and a page of reply (feature 044). */
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);


/* The adapter the connection is opened on: found by the caller, as the suite
 * finds rvk among every adapter D3DKMT enumerates. */
std::uint32_t found_adapter(MockKmt &kmt)
{
    std::uint32_t adapters[1] = {};
    std::uint32_t count = 0u;
    static_cast<void>(kmt.enumerate_adapters(adapters, 1u, count));
    return adapters[0];
}

class Fixture {
public:
    Fixture() : connection_(kmt_) { kmt_.add_adapter(rvk, 3200u); }
    MockKmt         &kmt() { return kmt_; }
    GuestConnection &connection() { return connection_; }
    Status open() { return connection_.open(found_adapter(kmt_)); }

private:
    MockKmt         kmt_;
    GuestConnection connection_;
};

}  // namespace

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

TEST(kmt_transport, initialize_maps_the_ring_and_creates_the_virtual_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.established());
  /* The record was carried as private data. */
  CHECK_EQ(fixture.kmt().last_private_size(), RvkContextRecord::encoded_size);
  /* Each allocation is the kind the driver pins in the aperture and publishes;
   * a ring of order 12 is two pages, its header and data. */
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::ring_allocation(transport)), 17u);
  CHECK_EQ(fixture.kmt().allocation_pages(KmtTransportProbe::ring_allocation(transport)), 2u);
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::fence_allocation(transport)), 18u);
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::reply_allocation(transport)), 19u);
  CHECK_EQ(fixture.kmt().allocation_kind(KmtTransportProbe::dma_allocation(transport)), 16u);
  RvkContextRecord record;
  CHECK_STATUS(record.decode(fixture.kmt().last_private(), fixture.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.ring(), KmtTransportProbe::ring_allocation(transport));
  CHECK_EQ(record.fence(), KmtTransportProbe::fence_allocation(transport));
  CHECK_EQ(record.reply(), KmtTransportProbe::reply_allocation(transport));
  /* A second initialize is refused. */
  CHECK_STATUS(transport.initialize(), Status::invalid_state);
  CHECK(transport.invariants_hold());
  transport.teardown();
  CHECK(!transport.established()); }

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
  const std::uint8_t *ring = fixture.kmt().view_of(KmtTransportProbe::ring_allocation(transport));
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
  CHECK_EQ(submitted.end(), 32u);   /* the batch, its command and 4 bytes, padded to 8 */ }

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
  std::uint8_t *reply = fixture.kmt().view_of(KmtTransportProbe::reply_allocation(transport));
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
    kmt.add_adapter(rvk, 3200u);
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
{ refuse_setup(MockKmt::Call::create_allocation, 1u);   /* ring */
  refuse_setup(MockKmt::Call::create_allocation, 2u);   /* fence */
  refuse_setup(MockKmt::Call::create_allocation, 3u);   /* reply */
  refuse_setup(MockKmt::Call::create_allocation, 4u);   /* dma */
  refuse_setup(MockKmt::Call::make_resident, 1u);
  refuse_setup(MockKmt::Call::wait_for_fence, 1u);      /* ring residency */
  refuse_setup(MockKmt::Call::map_gpu_virtual_address, 1u);
  refuse_setup(MockKmt::Call::lock, 1u);
  refuse_setup(MockKmt::Call::create_context_virtual, 1u); }

/* The last residency wait a setup makes is the DMA buffer's mapping wait; a
 * device that never reaches it fails the setup. Counting the waits keeps this
 * robust as the allocation count changes. */
TEST(kmt_transport, a_failed_mapping_wait_leaves_it_unestablished)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t counted_staging[staging_bytes] = {};
  KmtTransport counted(connection, SubApi::vulkan, 12u, reply_bytes, counted_staging, sizeof counted_staging);
  CHECK_STATUS(counted.initialize(), Status::ok);
  const std::uint32_t waits = kmt.calls(MockKmt::Call::wait_for_fence);
  counted.teardown();
  MockKmt fresh;
  fresh.add_adapter(rvk, 3200u);
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

/* A lock that hands back a null view cannot bind the ring. */
TEST(kmt_transport, a_null_ring_view_is_refused)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  kmt.zero_next(MockKmt::Call::lock);   /* the ring lock returns no view */
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK(transport.initialize() != Status::ok);
  CHECK(!transport.established()); }

/* A submission the device refuses fails the send. */
TEST(kmt_transport, a_refused_submission_fails_the_send)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  kmt.fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  WireWriter writer = transport.request_writer();
  writer.write_u32(1u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed); }

/* With every fence in flight the next send is refused. */
TEST(kmt_transport, a_full_fence_table_refuses_the_next_send)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
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
  WireWriter writer = transport.request_writer();
  writer.write_u32(64u);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed); }

/* A batch with no room in the ring is refused before the fence table fills. */
TEST(kmt_transport, a_full_ring_refuses_the_send)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  bool refused = false;
  for (std::uint32_t i = 0u; i < 64u && !refused; i++) {
      WireWriter writer = transport.request_writer();
      writer.write_zeros(900u);   /* a big command; a few fill an order-12 ring */
      refused = transport.send_async(0x10001u, writer) == TransportResult::failed;
  }
  CHECK(refused); }

/* send_sync fails a call that could not be published, a device that will not
 * wait, and an answer the device never wrote. */
TEST(kmt_transport, send_sync_failure_paths)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
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
  kmt.add_adapter(rvk, 3200u);
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

/* Teardown gives back the views setup took - the ring's and the reply's - and
 * nothing it never locked, leaving the connection as it found it. */
TEST(kmt_transport, teardown_unlocks_only_the_views_it_holds)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::lock), 2u);
  transport.teardown();
  CHECK(transport.invariants_hold());
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::unlock), 2u);
  CHECK_EQ(fixture.kmt().misuses(), 0u);
  /* The adapter, the device and the queue. */
  CHECK_EQ(fixture.kmt().open_objects(), 3u);
  /* A setup that failed after the ring's lock gives that one view back. */
  fixture.kmt().fail_at(MockKmt::Call::lock, 2u, MockKmt::invalid_parameter);
  CHECK(transport.initialize() != Status::ok);
  CHECK(transport.invariants_hold());
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::unlock), 3u);
  CHECK_EQ(fixture.kmt().misuses(), 0u);
  CHECK_EQ(fixture.kmt().open_objects(), 3u); }

TEST(kmt_transport, the_invariant_catches_a_view_of_no_allocation)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK(transport.invariants_hold());
  KmtTransportProbe::phantom_view(transport);
  CHECK(!transport.invariants_hold()); }

TEST(kmt_transport, the_invariant_catches_an_established_context_with_no_buffer)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
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
  CHECK_EQ(fixture.kmt().allocation_pages(KmtTransportProbe::ring_allocation(transport)), 512u);
  CHECK_EQ(fixture.kmt().allocation_pages(KmtTransportProbe::reply_allocation(transport)), 16u);
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
  const std::uint8_t *const ring = fixture.kmt().view_of(KmtTransportProbe::ring_allocation(transport));
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
      const std::uint8_t *const ring = fixture.kmt().view_of(KmtTransportProbe::ring_allocation(transport));
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
