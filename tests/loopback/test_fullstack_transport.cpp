/* L4 through the real driver: the 043 transport's round trip with nothing of
 * the miniport's job left in the harness. KmtTransport's calls reach the
 * registered DDI table through DdiKmt; the driver's AvContextTable publishes
 * the ring, fence page and reply slot and sets the host context up over the
 * control queue the real device core answers; a submission rings the doorbell
 * page the core's worker waits on, and the fence interrupt it raises reaches
 * the driver's ISR and DPC, whose DMA_COMPLETED is what the wait sees. */
#include "core/miniport/ddi_slot.hpp"
#include "core/miniport/ddi_table.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "proto/client_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_window_record.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <sched.h>

namespace av {

struct KmtTransportProbe {
    static void forget_fault(KmtTransport &transport) { transport.faulted_ = false; }
};

}  // namespace av

using av::DdiKmt;
using av::DdiSlot;
using av::DdiTable;
using av::FullStackRig;
using av::GuestConnection;
using av::HostWindowRecord;
using av::KmtTransport;
using av::KmtTransportProbe;
using av::MockExecutorOps;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;

namespace {

/* The av suite's transport: 1 KiB commands and a page of reply (feature 044). */
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;

/* The command TransportScenario sends; MockExecutorOps echoes every payload it
 * does not script, as tests/qemu/echo_executor.cpp does for this one. */
constexpr std::uint32_t echo_command = 0x10001u;
constexpr std::uint64_t echo_payload = 0x0123456789ABCDEFull;
constexpr std::uint8_t  ring_order = 12u;
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;
/* STATUS_INVALID_PARAMETER, what SubmitCommandVirtual answers a lost context. */
constexpr std::uint32_t invalid_parameter = 0xC000000Du;

/* One echo on `transport`: whether it came back as exactly the payload sent. */
bool echoes(KmtTransport &transport, std::uint64_t payload)
{
    WireWriter request = transport.request_writer();
    request.write_u64(payload);
    WireReader reply(nullptr, 0u);
    if (transport.send_sync(echo_command, request, reply) != TransportResult::ok) return false;
    return reply.remaining() == sizeof payload && reply.read_u64() == payload;
}

/* How many of the driver's fences have completed once every one `transport`
 * handed it has, or the rig gave up: a process that read its answer from its
 * own memory made no wait, and the driver's completion reaches the rig with
 * the next interrupt the rig delivers. */
std::uint32_t settled_fences(FullStackRig &rig, const KmtTransport &transport)
{
    constexpr std::uint32_t passes = 20000u;
    for (std::uint32_t pass = 0u; pass < passes; pass++) {
        if (rig.kmt().fences().completed() >= transport.submissions_through_kernel()) break;
        rig.pump().deliver();
        sched_yield();
    }
    return rig.kmt().fences().completed();
}

}  // namespace

TEST(fullstack_adapter, starts_on_the_real_device_core_with_its_capset_and_doorbells)
{ FullStackRig rig;
  CHECK_EQ(rig.driver().entered(), 0);
  CHECK_STATUS(rig.driver().registered(), Status::ok);
  CHECK_EQ(rig.added(), 0);
  CHECK_EQ(rig.started(), 0);
  CHECK(rig.fdo().started());
  CHECK(rig.platform().logged_line("avgpu: StartDevice"));
  /* The capset came from the device core, not from a scripted answer. */
  CHECK(rig.platform().logged_line("avgpu: device capset status 0x00000000"));
  /* The host GPU's name and memory, from that capset, are where Device
   * Manager, WMI and DXGI read an adapter's. */
  CHECK(rig.platform().logged_line("avgpu: adapter description status 0x00000000"));
  CHECK(rig.platform().described_device() != nullptr);
  CHECK(rig.platform().device_description_is("mock gpu"));
  CHECK(rig.platform().driver_text_is("HardwareInformation.AdapterString", "mock gpu"));
  CHECK_EQ(rig.platform().driver_number("HardwareInformation.qwMemorySize"), 24ull << 30);
  /* The hostmem region is the driver's host window, and nothing is placed in
   * it until a process asks (docs/host-window.md). */
  CHECK(rig.platform().logged_line("avgpu: host window status 0x00000000"));
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK(rig.bridge().answered_by_core() >= 2u);      /* the capset's two */
  CHECK(rig.bridge().answered_by_base() >= 1u);      /* GET_DISPLAY_INFO */
  CHECK_EQ(rig.fdo().children(), 1u);
  /* BAR 0's doorbell pages, the virtio BAR and the MSI-X table stay mapped. */
  CHECK(rig.dxgkrnl().live_mappings() >= 2u);
  CHECK_EQ(rig.device().doorbells().stores(), 0u); }

/* What a Vulkan ICD sizes its heaps and names its adapter by: the rig's LUID,
 * and budgets that are the driver's own segments - the aperture over the
 * guest's 8 GiB of system memory, which is not local, and the host GPU's 24 GiB
 * as the capset reports it, which is; the hostmem region is the driver's host
 * window, not a segment (docs/host-window.md) - with this process's
 * allocations counted where they live, none of them in the host GPU's. */
TEST(fullstack_adapter, the_luid_and_the_video_memory_budgets_are_the_drivers_segments)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt &kmt = rig.kmt();
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint64_t luid = 0u;
  CHECK_STATUS(kmt.adapter_luid(adapter, luid), Status::ok);
  CHECK_EQ(luid, DdiKmt::rig_luid);
  CHECK_STATUS(kmt.adapter_luid(adapter + 1u, luid), Status::invalid_parameter);
  CHECK_EQ(luid, 0u);
  std::uint64_t budget = 0u;
  std::uint64_t usage = 1u;
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::local, budget, usage), Status::ok);
  CHECK_EQ(budget, std::uint64_t{24} << 30u);
  CHECK_EQ(usage, 0u);
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::non_local, budget, usage), Status::ok);
  CHECK_EQ(budget, std::uint64_t{8} << 30u);
  CHECK_EQ(usage, 0u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, ring_order,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::non_local, budget, usage), Status::ok);
  /* The page naming the context and the DMA buffer: the ring, fence page and
   * reply slot are the driver's own pages, which no segment counts. */
  CHECK_EQ(usage, 2u * 4096u);
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::local, budget, usage), Status::ok);
  CHECK_EQ(usage, 0u);
  CHECK_STATUS(kmt.query_video_memory(adapter + 1u, av::MemorySegmentGroup::local, budget, usage),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.query_video_memory(adapter, static_cast<av::MemorySegmentGroup>(2u), budget, usage),
               Status::invalid_parameter);
  CHECK(budget == 0u && usage == 0u);
  transport.teardown();
  connection.close();
  CHECK_EQ(kmt.open_objects(), 0u); }

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
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, ring_order,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.established());
  CHECK(rig.platform().logged_line("avgpu: av context on doorbell 0 message 3 status 0x00000000"));
  CHECK_EQ(rig.device().device().contexts(), 1u);

  const std::uint32_t executed = rig.device().executor().executed();
  CHECK(echoes(transport, echo_payload));
  CHECK(rig.device().executor().executed() > executed);
  CHECK_EQ(rig.device().executor().id_at(executed), echo_command);
  CHECK(rig.device().doorbells().stores() >= 1u);
  /* A second batch on the same ring, after the first moved its head. */
  CHECK(echoes(transport, ~echo_payload));
  /* The process publishes its tail first and calls the kernel only for a
   * worker that is asleep: every batch it did hand the driver completed. */
  CHECK_EQ(transport.submissions(), 2u);
  CHECK_EQ(settled_fences(rig, transport), transport.submissions_through_kernel());

  CHECK_EQ(rig.device().device().blobs(), 3u);   /* the ring, fence page and reply slot */
  transport.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  /* The display clock's page and the present engine's, the device's while it runs. */
  CHECK_EQ(rig.device().host().live_guest_mappings(), 2u);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.handles().outstanding(), 0u); }

/* A process takes a chunk of its device's host window through the real driver
 * and the real device core (docs/host-window.md): a host blob of the context's
 * own range, placed at the window's first page, and the process's view of it.
 * Giving it back takes all three away again. */
TEST(fullstack_transport, an_escape_opens_a_host_window_chunk_through_the_real_driver)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt &kmt = rig.kmt();
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, ring_order,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK_EQ(rig.device().device().blobs(), 3u);

  std::uint8_t bytes[HostWindowRecord::encoded_size] = {};
  HostWindowRecord open(HostWindowRecord::Operation::open, 0xF800u, 0u);
  CHECK_STATUS(open.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(open.accept_answer(bytes, sizeof bytes), Status::ok);
  CHECK(open.resource_id() != 0u);
  CHECK_EQ(open.bytes(), 0x10000u);
  CHECK(open.address() != 0u);
  CHECK_EQ(rig.device().device().blobs(), 4u);
  CHECK(rig.device().host().placed_at(0u) != nullptr);
  CHECK_EQ(rig.platform().process_mappings(), 3u);   /* and the context's ring and reply slot */
  /* The process's view is the host's memory: what it writes, the host reads. */
  auto *const view = reinterpret_cast<volatile std::uint8_t *>(static_cast<std::uintptr_t>(open.address()));
  view[0] = 0x5Au;
  view[0xF7FFu] = 0xA5u;
  CHECK_EQ(static_cast<const std::uint8_t *>(rig.device().host().placed_at(0u))[0], 0x5Au);
  CHECK_EQ(static_cast<const std::uint8_t *>(rig.device().host().placed_at(0u))[0xF7FFu], 0xA5u);

  HostWindowRecord close(HostWindowRecord::Operation::close, 0u, open.resource_id());
  CHECK_STATUS(close.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_EQ(rig.device().device().blobs(), 3u);
  CHECK(rig.device().host().placed_at(0u) == nullptr);
  CHECK_EQ(rig.platform().process_mappings(), 2u);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes),
               Status::invalid_parameter);

  /* One left open is closed with its context. */
  HostWindowRecord again(HostWindowRecord::Operation::open, 0x1000u, 0u);
  CHECK_STATUS(again.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_EQ(rig.device().device().blobs(), 4u);
  transport.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

/* DdiKmt's escape is DxgkDdiEscape as dxgkrnl calls it: the driver's device and
 * context, a kernel copy of the record copied back out. A chunk larger than the
 * whole window is guest RAM the driver owns instead, a GUEST blob of the
 * context's own range the device imports, and the process's view is that
 * memory; the close gives the blob back. Handles the process never had, and
 * data that is no record, are refused. */
TEST(fullstack_transport, a_chunk_the_window_cannot_hold_is_guest_ram_through_the_real_driver)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt &kmt = rig.kmt();
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, ring_order,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  const std::uint32_t mapped = rig.device().host().live_guest_mappings();
  const std::uint64_t chunk_bytes = FullStackRig::hostmem_bytes + 0x1000u;

  std::uint8_t bytes[HostWindowRecord::encoded_size] = {};
  HostWindowRecord open(HostWindowRecord::Operation::open, chunk_bytes, 0u);
  CHECK_STATUS(open.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context() + 1u, bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(adapter, connection.device() + 1u, transport.context(), bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), nullptr, sizeof bytes),
               Status::invalid_parameter);
  CHECK_EQ(rig.device().device().blobs(), 3u);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(open.accept_answer(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(open.bytes(), chunk_bytes);
  CHECK(open.resource_id() != 0u);
  CHECK(rig.device().host().placed_at(0u) == nullptr);   /* nothing of the window */
  CHECK_EQ(rig.device().device().blobs(), 4u);
  CHECK(rig.device().host().live_guest_mappings() > mapped);
  CHECK_EQ(rig.platform().process_mappings(), 3u);
  auto *const view = reinterpret_cast<volatile std::uint8_t *>(static_cast<std::uintptr_t>(open.address()));
  view[chunk_bytes - 1u] = 0x5Au;
  CHECK_EQ(view[chunk_bytes - 1u], 0x5Au);

  std::uint8_t foreign[HostWindowRecord::encoded_size] = {'N', 'O', 'P', 'E'};
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), foreign, sizeof foreign),
               Status::not_supported);
  HostWindowRecord close(HostWindowRecord::Operation::close, 0u, open.resource_id());
  CHECK_STATUS(close.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_EQ(rig.device().device().blobs(), 3u);
  CHECK_EQ(rig.device().host().live_guest_mappings(), mapped);
  CHECK_EQ(rig.platform().process_mappings(), 2u);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes),
               Status::invalid_parameter);
  transport.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), 0u);
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
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, ring_order,
                         reply_bytes, transport_staging, sizeof transport_staging);
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
  CHECK_EQ(settled_fences(rig, transport), transport.submissions_through_kernel());

  CHECK(echoes(transport, echo_payload));
  transport.teardown();
  connection.close(); }

/* FR-006 through the whole stack: a batch the executor cannot decode faults
 * its context on the host; the driver completes the submission, so the
 * process's wait returns and finds no reply, and refuses the context's next
 * submission; a context beside it on the same device keeps running. The
 * driver never reports DMA_FAULTED, which dxgkrnl bugchecks on (0x119/0xD). */
TEST(fullstack_transport, a_faulted_context_fails_its_wait_and_its_neighbour_still_echoes)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt &kmt = rig.kmt();
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  std::uint8_t doomed_staging[staging_bytes] = {};
  KmtTransport doomed(connection, SubApi::vulkan, ring_order, reply_bytes, doomed_staging, sizeof doomed_staging);
  std::uint8_t healthy_staging[staging_bytes] = {};
  KmtTransport healthy(connection, SubApi::vulkan, ring_order, reply_bytes, healthy_staging, sizeof healthy_staging);
  CHECK_STATUS(doomed.initialize(), Status::ok);
  CHECK_STATUS(healthy.initialize(), Status::ok);
  CHECK(rig.platform().logged_line("avgpu: av context on doorbell 1 message 4 status 0x00000000"));

  WireWriter request = doomed.request_writer();
  request.write_u64(echo_payload);
  WireReader reply(nullptr, 0u);
  const std::uint32_t completed = kmt.fences().completed();
  const std::uint64_t through_kernel = doomed.submissions_through_kernel();
  CHECK(doomed.send_sync(MockExecutorOps::undecodable_id, request, reply) == TransportResult::failed);
  CHECK(doomed.faulted());
  /* The wait completed - through the driver, when the batch went that way -
   * and the reply is what is missing. */
  CHECK_EQ(kmt.fences().completed(), completed + (doomed.submissions_through_kernel() - through_kernel));
  CHECK_EQ(kmt.last_code(), 0u);
  /* With the transport's own latch lifted the send reaches the driver, which
   * refuses it before the doorbell rings. */
  const std::uint32_t stores = rig.device().doorbells().stores();
  KmtTransportProbe::forget_fault(doomed);
  CHECK(!echoes(doomed, echo_payload));
  CHECK_EQ(kmt.last_code(), invalid_parameter);
  CHECK_EQ(rig.device().doorbells().stores(), stores);
  CHECK(doomed.faulted());

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
  std::uint8_t before_staging[staging_bytes] = {};
  KmtTransport before(connection, SubApi::vulkan, ring_order, reply_bytes, before_staging, sizeof before_staging);
  CHECK_STATUS(before.initialize(), Status::ok);
  CHECK(echoes(before, echo_payload));

  CHECK_EQ(rig.fdo().set_power_state(power_d3), 0);
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(rig.fdo().set_power_state(power_d0), 0);
  CHECK(rig.platform().logged_line("avgpu: SetPowerState D0 done"));
  /* The same device came back with nothing placed in its window. */
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK(!echoes(before, echo_payload));
  CHECK(before.faulted());
  before.teardown();

  std::uint8_t after_staging[staging_bytes] = {};
  KmtTransport after(connection, SubApi::vulkan, ring_order, reply_bytes, after_staging, sizeof after_staging);
  CHECK_STATUS(after.initialize(), Status::ok);
  CHECK(echoes(after, echo_payload));
  after.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* A timeout reset is a device reset: dxgkrnl's ResetFromTimeout loses every
 * context, and RestartFromTimeout brings back a device with room for a new
 * transport. */
TEST(fullstack_transport, a_timeout_reset_loses_the_context_and_the_restart_runs_a_new_one)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  GuestConnection connection(rig.kmt());
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  std::uint8_t before_staging[staging_bytes] = {};
  KmtTransport before(connection, SubApi::vulkan, ring_order, reply_bytes, before_staging, sizeof before_staging);
  CHECK_STATUS(before.initialize(), Status::ok);
  CHECK(echoes(before, echo_payload));

  const DdiTable &table = rig.driver().table();
  rig.platform().set_irql(av::Irql::passive);
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *)>(table.routine(DdiSlot::DxgkDdiResetFromTimeout))(
               rig.fdo().context()),
           0);
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *)>(table.routine(DdiSlot::DxgkDdiRestartFromTimeout))(
               rig.fdo().context()),
           0);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK(!echoes(before, echo_payload));
  before.teardown();

  std::uint8_t after_staging[staging_bytes] = {};
  KmtTransport after(connection, SubApi::vulkan, ring_order, reply_bytes, after_staging, sizeof after_staging);
  CHECK_STATUS(after.initialize(), Status::ok);
  CHECK(echoes(after, echo_payload));
  after.teardown();
  connection.close(); }

/* pnputil disabling and enabling the adapter under a process that keeps its
 * device: StopDevice resets the device and StartDevice builds a new one, on
 * which a new transport runs. */
TEST(fullstack_transport, a_stop_and_start_under_a_live_transport_leaves_room_for_a_new_one)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  GuestConnection connection(rig.kmt());
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  std::uint8_t before_staging[staging_bytes] = {};
  KmtTransport before(connection, SubApi::vulkan, ring_order, reply_bytes, before_staging, sizeof before_staging);
  CHECK_STATUS(before.initialize(), Status::ok);
  CHECK(echoes(before, echo_payload));

  CHECK_EQ(rig.fdo().stop_device(), 0);
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(rig.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(rig.fdo().start_device(), 0);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK(!echoes(before, echo_payload));
  before.teardown();

  std::uint8_t after_staging[staging_bytes] = {};
  KmtTransport after(connection, SubApi::vulkan, ring_order, reply_bytes, after_staging, sizeof after_staging);
  CHECK_STATUS(after.initialize(), Status::ok);
  CHECK(echoes(after, echo_payload));
  after.teardown();
  connection.close(); }

/* Thousands of batches on the smallest ring, so its head and tail wrap many
 * times with the driver publishing and the device core consuming. */
TEST(fullstack_transport, the_smallest_ring_wraps_many_times_and_every_echo_returns)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  constexpr std::uint32_t batches = 3000u;
  GuestConnection connection(rig.kmt());
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, ring_order,
                         reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  std::uint32_t returned = 0u;
  for (std::uint32_t batch = 0u; batch < batches; batch++)
      returned += echoes(transport, echo_payload ^ batch) ? 1u : 0u;
  CHECK_EQ(returned, batches);
  CHECK_EQ(transport.submissions(), batches);
  CHECK_EQ(settled_fences(rig, transport), transport.submissions_through_kernel());
  transport.teardown();
  connection.close(); }

/* Five transports interleaved on one device, each on a doorbell and a message
 * of its own; one torn down gives its doorbell to the next set up. */
TEST(fullstack_transport, five_transports_interleave_and_a_freed_doorbell_is_reused)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  constexpr std::uint32_t transports = 5u;
  constexpr std::uint32_t rounds = 50u;
  GuestConnection connection(rig.kmt());
  CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
  std::uint8_t first_staging[staging_bytes] = {};
  KmtTransport first(connection, SubApi::vulkan, ring_order, reply_bytes, first_staging, sizeof first_staging);
  std::uint8_t second_staging[staging_bytes] = {};
  KmtTransport second(connection, SubApi::vulkan, ring_order, reply_bytes, second_staging, sizeof second_staging);
  std::uint8_t third_staging[staging_bytes] = {};
  KmtTransport third(connection, SubApi::vulkan, ring_order, reply_bytes, third_staging, sizeof third_staging);
  std::uint8_t fourth_staging[staging_bytes] = {};
  KmtTransport fourth(connection, SubApi::vulkan, ring_order, reply_bytes, fourth_staging, sizeof fourth_staging);
  std::uint8_t fifth_staging[staging_bytes] = {};
  KmtTransport fifth(connection, SubApi::vulkan, ring_order, reply_bytes, fifth_staging, sizeof fifth_staging);
  KmtTransport *const all[transports] = {&first, &second, &third, &fourth, &fifth};
  for (KmtTransport *transport : all) CHECK_STATUS(transport->initialize(), Status::ok);
  CHECK(rig.platform().logged_line("avgpu: av context on doorbell 4 message 7 status 0x00000000"));
  CHECK_EQ(rig.device().device().contexts(), transports);
  std::uint32_t returned = 0u;
  for (std::uint32_t round = 0u; round < rounds; round++)
      for (KmtTransport *transport : all) returned += echoes(*transport, echo_payload ^ round) ? 1u : 0u;
  CHECK_EQ(returned, transports * rounds);

  third.teardown();
  std::uint8_t reused_staging[staging_bytes] = {};
  KmtTransport reused(connection, SubApi::vulkan, ring_order, reply_bytes, reused_staging, sizeof reused_staging);
  CHECK_STATUS(reused.initialize(), Status::ok);
  CHECK_EQ(rig.device().host().live_doorbells(), transports);
  CHECK(echoes(reused, echo_payload));
  CHECK(echoes(second, echo_payload));
  for (KmtTransport *transport : all) transport->teardown();
  reused.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_teardown, a_process_that_exits_holding_a_context_leaves_nothing_behind)
{ { FullStackRig rig;
    CHECK_EQ(rig.started(), 0);
    GuestConnection connection(rig.kmt());
    CHECK_STATUS(connection.open(DdiKmt::adapter_handle), Status::ok);
    std::uint8_t transport_staging[staging_bytes] = {};
    KmtTransport transport(connection, SubApi::vulkan, ring_order,
                           reply_bytes, transport_staging, sizeof transport_staging);
    CHECK_STATUS(transport.initialize(), Status::ok);
    CHECK(echoes(transport, echo_payload));
    CHECK(rig.kmt().open_objects() >= 4u);   /* a device, two allocations and a context */
    /* The process ends here without a teardown: dxgkrnl cleans up after it. */
    rig.kmt().end_process();
    CHECK_EQ(rig.kmt().open_objects(), 0u);
    CHECK_EQ(rig.device().device().contexts(), 0u);
    CHECK_EQ(rig.platform().mapped_frames(), 0u);
    CHECK_EQ(rig.platform().unclaimed_violations(), 0u);
    connection.close(); } }
