/* L4 through the real driver: the 043 transport's round trip with nothing of
 * the miniport's job left in the harness. KmtTransport's calls reach the
 * registered DDI table through DdiKmt; the driver's RvkContextTable publishes
 * the ring, fence page and reply slot and sets the host context up over the
 * control queue the real device core answers; a submission rings the doorbell
 * page the core's worker waits on, and the fence interrupt it raises reaches
 * the driver's ISR and DPC, whose DMA_COMPLETED is what the wait sees. */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "proto/client_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::DdiKmt;
using av::FullStackRig;
using av::GuestConnection;
using av::KmtTransport;
using av::MockExecutorOps;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;

namespace {

/* The command TransportScenario sends; MockExecutorOps echoes every payload it
 * does not script, as tests/qemu/echo_executor.cpp does for this one. */
constexpr std::uint32_t echo_command = 0x10001u;
constexpr std::uint64_t echo_payload = 0x0123456789ABCDEFull;
constexpr std::uint8_t  ring_order = 12u;
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;

/* One echo on `transport`: whether it came back as exactly the payload sent. */
bool echoes(KmtTransport &transport, std::uint64_t payload)
{
    WireWriter request = transport.request_writer();
    request.write_u64(payload);
    WireReader reply(nullptr, 0u);
    if (transport.send_sync(echo_command, request, reply) != TransportResult::ok) return false;
    return reply.remaining() == sizeof payload && reply.read_u64() == payload;
}

}  // namespace

TEST(fullstack_adapter, starts_on_the_real_device_core_with_its_capset_and_doorbells)
{ FullStackRig rig;
  CHECK_EQ(rig.driver().entered(), 0);
  CHECK_STATUS(rig.driver().registered(), Status::ok);
  CHECK_EQ(rig.added(), 0);
  CHECK_EQ(rig.started(), 0);
  CHECK(rig.fdo().started());
  CHECK(rig.platform().logged_line("rvkgpu: StartDevice"));
  /* The capset came from the device core, not from a scripted answer. */
  CHECK(rig.platform().logged_line("rvkgpu: device capset status 0x00000000"));
  /* The host-visible region is one blob the device core made and placed. */
  CHECK(rig.platform().logged_line("rvkgpu: host-visible region status 0x00000000"));
  CHECK_EQ(rig.device().device().blobs(), 1u);
  CHECK(rig.device().host().placed_at(0u) != nullptr);
  CHECK(rig.bridge().answered_by_core() >= 4u);      /* the capset's two, CREATE_BLOB and MAP_BLOB */
  CHECK(rig.bridge().answered_by_base() >= 1u);      /* GET_DISPLAY_INFO */
  CHECK_EQ(rig.fdo().children(), 1u);
  /* BAR 0's doorbell pages, the virtio BAR and the MSI-X table stay mapped. */
  CHECK(rig.dxgkrnl().live_mappings() >= 2u);
  CHECK_EQ(rig.device().doorbells().stores(), 0u); }

TEST(fullstack_transport, an_echo_crosses_the_real_driver_and_the_real_device_core)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt &kmt = rig.kmt();
  std::uint32_t adapters[2] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 2u, count), Status::ok);
  CHECK_EQ(count, 1u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapters[0]), Status::ok);
  KmtTransport transport(connection, SubApi::vulkan, ring_order);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.established());
  CHECK(rig.platform().logged_line("rvkgpu: rvk context on doorbell 0 message 3 status 0x00000000"));
  CHECK_EQ(rig.device().device().contexts(), 1u);

  const std::uint32_t executed = rig.device().executor().executed();
  CHECK(echoes(transport, echo_payload));
  CHECK(rig.device().executor().executed() > executed);
  CHECK_EQ(rig.device().executor().id_at(executed), echo_command);
  CHECK(rig.device().doorbells().stores() >= 1u);
  CHECK_EQ(kmt.fences().faults(), 0u);
  /* A second batch on the same ring, after the first moved its head. */
  CHECK(echoes(transport, ~echo_payload));
  CHECK_EQ(kmt.fences().completed(), 2u);

  CHECK_EQ(rig.device().device().blobs(), 4u);   /* the region, and the ring, fence page and reply slot */
  transport.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 1u);
  CHECK_EQ(rig.device().host().live_guest_mappings(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.handles().outstanding(), 0u); }

/* The completion reaches dxgkrnl only through the fence interrupt: while the
 * executor holds the batch the DPC the submission queued finds nothing
 * finished, and after the worker raises the fence queue nothing changes until
 * that interrupt is delivered to the driver's ISR and DPC. */
TEST(fullstack_transport, a_batch_the_host_finishes_later_completes_on_its_fence_interrupt)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt &kmt = rig.kmt();
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  KmtTransport transport(connection, SubApi::vulkan, ring_order);
  CHECK_STATUS(transport.initialize(), Status::ok);

  rig.device().executor().hold();
  WireWriter request = transport.request_writer();
  request.write_u64(echo_payload);
  CHECK(transport.send_async(echo_command, request) == TransportResult::ok);
  CHECK(rig.device().holding(1u));
  rig.pump().deliver();
  CHECK_EQ(kmt.fences().completed(), 0u);

  const std::uint32_t raised = rig.device().host().raised(0u);
  rig.device().executor().release();
  CHECK(rig.device().raised_past(0u, raised));
  CHECK_EQ(kmt.fences().completed(), 0u);
  const std::uint32_t delivered = rig.pump().delivered();
  rig.pump().deliver();
  CHECK(rig.pump().delivered() > delivered);
  CHECK_EQ(kmt.fences().completed(), 1u);

  CHECK(echoes(transport, echo_payload));
  transport.teardown();
  connection.close(); }

/* FR-006 through the whole stack: a batch the executor cannot decode faults
 * its context on the host, the driver reports DMA_FAULTED for it, and the
 * process's wait fails; a context beside it on the same device keeps running. */
TEST(fullstack_transport, a_faulted_context_fails_its_wait_and_its_neighbour_still_echoes)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt &kmt = rig.kmt();
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  KmtTransport doomed(connection, SubApi::vulkan, ring_order);
  KmtTransport healthy(connection, SubApi::vulkan, ring_order);
  CHECK_STATUS(doomed.initialize(), Status::ok);
  CHECK_STATUS(healthy.initialize(), Status::ok);
  CHECK(rig.platform().logged_line("rvkgpu: rvk context on doorbell 1 message 4 status 0x00000000"));

  WireWriter request = doomed.request_writer();
  request.write_u64(echo_payload);
  WireReader reply(nullptr, 0u);
  CHECK(doomed.send_sync(MockExecutorOps::undecodable_id, request, reply) == TransportResult::failed);
  CHECK(doomed.faulted());
  CHECK_EQ(kmt.fences().faults(), 1u);
  CHECK(!echoes(doomed, echo_payload));

  CHECK(echoes(healthy, echo_payload));
  CHECK(!healthy.faulted());

  doomed.teardown();
  healthy.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* Feature 040 through the whole stack: D3 resets the device, which destroys
 * every host context, so the process's transport is lost - its next send
 * fails rather than waiting on a fence nobody will write - and a transport it
 * sets up again after D0 runs on the device that came back. */
TEST(fullstack_transport, a_power_cycle_loses_the_context_and_a_new_one_echoes)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  GuestConnection connection(rig.kmt());
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  KmtTransport before(connection, SubApi::vulkan, ring_order);
  CHECK_STATUS(before.initialize(), Status::ok);
  CHECK(echoes(before, echo_payload));

  CHECK_EQ(rig.fdo().set_power_state(power_d3), 0);
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(rig.fdo().set_power_state(power_d0), 0);
  CHECK(rig.platform().logged_line("rvkgpu: SetPowerState D0 done"));
  /* The same device came back, so the host-visible region is made on it again. */
  CHECK_EQ(rig.device().device().blobs(), 1u);
  CHECK(!echoes(before, echo_payload));
  CHECK(before.faulted());
  before.teardown();

  KmtTransport after(connection, SubApi::vulkan, ring_order);
  CHECK_STATUS(after.initialize(), Status::ok);
  CHECK(echoes(after, echo_payload));
  after.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_teardown, a_process_that_exits_holding_a_context_leaves_nothing_behind)
{ { FullStackRig rig;
    CHECK_EQ(rig.started(), 0);
    GuestConnection connection(rig.kmt());
    CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
    KmtTransport transport(connection, SubApi::vulkan, ring_order);
    CHECK_STATUS(transport.initialize(), Status::ok);
    CHECK(echoes(transport, echo_payload));
    CHECK(rig.kmt().open_objects() >= 6u);   /* a device, four allocations and a context */
    /* The process ends here without a teardown: dxgkrnl cleans up after it. */
    rig.kmt().end_process();
    CHECK_EQ(rig.kmt().open_objects(), 0u);
    CHECK_EQ(rig.device().device().contexts(), 0u);
    CHECK_EQ(rig.platform().mapped_frames(), 0u);
    CHECK_EQ(rig.platform().unclaimed_violations(), 0u);
    connection.close(); } }
