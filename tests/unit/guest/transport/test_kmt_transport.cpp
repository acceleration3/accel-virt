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
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
  CHECK(!transport.established());
  CHECK(transport.invariants_hold()); }

TEST(kmt_transport, initialize_maps_the_ring_and_creates_the_virtual_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
  CHECK_STATUS(transport.initialize(), Status::ok);
  WireWriter writer = transport.request_writer();
  writer.write_zeros(KmtTransport::max_command + 1u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(transport.send_async(0x10001u, writer) == TransportResult::failed); }

/* A synchronous command reads back the answer the device wrote for its fence. */
TEST(kmt_transport, a_sync_command_reads_the_reply_the_device_wrote)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
  CHECK_EQ(got.read_u32(), 2u); }

/* Sending before setup is refused; the decode arena and blob arena refuse. */
TEST(kmt_transport, before_setup_and_the_refusing_arenas)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
    KmtTransport transport(connection, SubApi::vulkan, 12u);
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
  KmtTransport counted(connection, SubApi::vulkan, 12u);
  CHECK_STATUS(counted.initialize(), Status::ok);
  const std::uint32_t waits = kmt.calls(MockKmt::Call::wait_for_fence);
  counted.teardown();
  MockKmt fresh;
  fresh.add_adapter(rvk, 3200u);
  GuestConnection other(fresh);
  CHECK_STATUS(other.open(found_adapter(fresh)), Status::ok);
  fresh.fail_at(MockKmt::Call::wait_for_fence, waits, MockKmt::invalid_parameter);
  KmtTransport transport(other, SubApi::vulkan, 12u);
  CHECK(transport.initialize() != Status::ok);
  CHECK(!transport.established()); }

/* Teardown leaves nothing bound: the same transport sets up a new context,
 * whose first fence is 1 again and whose ring starts empty, and a transport
 * whose setup failed after binding its ring can try again. */
TEST(kmt_transport, a_torn_down_transport_initializes_again)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
  KmtTransport transport(connection, SubApi::vulkan, 12u);
  CHECK(transport.initialize() != Status::ok);
  CHECK(!transport.established()); }

/* A submission the device refuses fails the send. */
TEST(kmt_transport, a_refused_submission_fails_the_send)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  KmtTransport transport(connection, SubApi::vulkan, 12u);
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
  KmtTransport transport(connection, SubApi::vulkan, 12u);
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
  KmtTransport transport(connection, SubApi::vulkan, 12u);
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
  KmtTransport transport(connection, SubApi::vulkan, 12u);
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
  KmtTransport transport(connection, SubApi::vulkan, 12u);
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
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
  CHECK(transport.invariants_hold());
  KmtTransportProbe::phantom_view(transport);
  CHECK(!transport.invariants_hold()); }

TEST(kmt_transport, the_invariant_catches_an_established_context_with_no_buffer)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(found_adapter(kmt)), Status::ok);
  KmtTransport transport(connection, SubApi::vulkan, 12u);
  KmtTransportProbe::orphan(transport);
  CHECK(!transport.invariants_hold()); }

/* A device-side failure loses the context: every later send fails, even a call
 * the device would now accept. */
TEST(kmt_transport, a_fault_loses_the_context_for_every_later_send)
{ Fixture fixture;
  CHECK_STATUS(fixture.open(), Status::ok);
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
  KmtTransport transport(fixture.connection(), SubApi::vulkan, 12u);
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
