#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/escape/host_window_record.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <sched.h>

namespace av {

struct KmtTransportProbe {
    static void forget_fault(KmtTransport &transport) { transport.faulted_ = false; }
};

}

using av::DdiKmt;
using av::DdiSlot;
using av::DdiTable;
using av::FullStackRig;
using av::GuestConnection;
using av::HostMemoryRequest;
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
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;

constexpr std::uint32_t echo_command = 0x10001u;
constexpr std::uint64_t echo_payload = 0x0123456789ABCDEFull;
constexpr std::uint8_t  ring_order = 12u;
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;
constexpr std::uint32_t invalid_parameter = 0xC000000Du;

bool echoes(KmtTransport &transport, std::uint64_t payload)
{
    WireWriter request = transport.request_writer();
    request.write_u64(payload);
    WireReader reply(nullptr, 0u);
    if (transport.send_sync(echo_command, request, reply) != TransportResult::ok) return false;
    return reply.remaining() == sizeof payload && reply.read_u64() == payload;
}

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

}

TEST(fullstack_adapter, starts_on_the_real_device_core_with_its_capset_and_doorbells)
{ FullStackRig rig;
  CHECK_EQ(rig.driver().entered(), 0);
  CHECK_STATUS(rig.driver().registered(), Status::ok);
  CHECK_EQ(rig.added(), 0);
  CHECK_EQ(rig.started(), 0);
  CHECK(rig.fdo().started());
  CHECK(rig.platform().logged_line("avgpu: StartDevice"));
  CHECK(rig.platform().logged_line("avgpu: device capset status 0x00000000"));
  CHECK(rig.platform().logged_line("avgpu: adapter description status 0x00000000"));
  CHECK(rig.platform().described_device() != nullptr);
  CHECK(rig.platform().device_description_is("mock gpu"));
  CHECK(rig.platform().driver_text_is("HardwareInformation.AdapterString", "mock gpu"));
  CHECK_EQ(rig.platform().driver_number("HardwareInformation.qwMemorySize"), 24ull << 30);
  CHECK(rig.platform().logged_line("avgpu: host window status 0x00000000"));
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK(rig.bridge().answered_by_core() >= 2u);
  CHECK(rig.bridge().answered_by_base() >= 1u);
  CHECK_EQ(rig.fdo().children(), 1u);
  CHECK(rig.dxgkrnl().live_mappings() >= 2u);
  CHECK_EQ(rig.device().doorbells().stores(), 0u); }

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
  CHECK(echoes(transport, ~echo_payload));
  CHECK_EQ(transport.submissions(), 2u);
  CHECK_EQ(settled_fences(rig, transport), transport.submissions_through_kernel());

  CHECK_EQ(rig.device().device().blobs(), 3u);
  transport.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(rig.device().host().live_guest_mappings(), 3u);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.handles().outstanding(), 0u); }

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
  MockExecutorOps &executor = rig.device().executor();
  const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);

  std::uint8_t bytes[HostWindowRecord::encoded_size] = {};
  HostWindowRecord open(HostWindowRecord::Operation::open, 0xF800u, 0u, upload);
  CHECK_STATUS(open.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(open.accept_answer(bytes, sizeof bytes), Status::ok);
  CHECK(open.resource_id() != 0u);
  CHECK_EQ(open.bytes(), 0x10000u);
  CHECK(open.address() != 0u);
  CHECK_EQ(rig.device().device().blobs(), 3u);
  CHECK_EQ(executor.held_memory(), 1u);
  CHECK_EQ(executor.last_memory_request().key, std::uint64_t{open.resource_id()});
  CHECK_EQ(executor.last_memory_request().size, 0x10000u);
  CHECK_EQ(executor.last_memory_request().request, upload.encoded());
  CHECK(rig.device().host().placed_at(0u) != nullptr);
  CHECK_EQ(rig.device().host().live_placements(), 1u);
  CHECK_EQ(rig.platform().process_mappings(), 4u);
  auto *const view = reinterpret_cast<volatile std::uint8_t *>(static_cast<std::uintptr_t>(open.address()));
  view[0] = 0x5Au;
  view[0xF7FFu] = 0xA5u;
  const std::uint8_t *const host = executor.memory_bytes(executor.last_opened_domain(), open.resource_id());
  CHECK(host != nullptr);
  CHECK_EQ(host[0], 0x5Au);
  CHECK_EQ(host[0xF7FFu], 0xA5u);

  HostWindowRecord close(HostWindowRecord::Operation::close, 0u, open.resource_id(), HostMemoryRequest());
  CHECK_STATUS(close.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK(rig.device().host().placed_at(0u) == nullptr);
  CHECK_EQ(rig.device().host().live_placements(), 0u);
  CHECK_EQ(executor.held_memory(), 0u);
  CHECK_EQ(executor.live_memory(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), 3u);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes),
               Status::invalid_parameter);

  HostWindowRecord again(HostWindowRecord::Operation::open, 0x1000u, 0u, upload);
  CHECK_STATUS(again.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_EQ(executor.held_memory(), 1u);
  transport.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(executor.held_memory(), 0u);
  CHECK_EQ(rig.device().host().live_placements(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(fullstack_transport, a_chunk_the_window_cannot_hold_is_refused_through_the_real_driver)
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
  MockExecutorOps    &executor = rig.device().executor();
  const std::uint32_t asked = executor.memory_allocations();

  std::uint8_t bytes[HostWindowRecord::encoded_size] = {};
  HostWindowRecord open(HostWindowRecord::Operation::open, FullStackRig::hostmem_bytes + 0x1000u, 0u,
                        HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u));
  CHECK_STATUS(open.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context() + 1u, bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(adapter, connection.device() + 1u, transport.context(), bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), nullptr, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_EQ(executor.memory_allocations(), asked);
  CHECK_EQ(executor.held_memory(), 0u);
  CHECK_EQ(rig.device().host().live_placements(), 0u);
  CHECK_EQ(rig.device().host().live_guest_mappings(), mapped);
  CHECK_EQ(rig.platform().process_mappings(), 3u);

  HostWindowRecord whole(HostWindowRecord::Operation::open, FullStackRig::hostmem_bytes, 0u,
                         HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u));
  CHECK_STATUS(whole.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(whole.accept_answer(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(whole.bytes(), FullStackRig::hostmem_bytes);
  CHECK(rig.device().host().placed_at(0u) != nullptr);

  std::uint8_t foreign[HostWindowRecord::encoded_size] = {'N', 'O', 'P', 'E'};
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), foreign, sizeof foreign),
               Status::not_supported);
  HostWindowRecord close(HostWindowRecord::Operation::close, 0u, whole.resource_id(), HostMemoryRequest());
  CHECK_STATUS(close.encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes), Status::ok);
  CHECK_EQ(rig.device().host().live_placements(), 0u);
  CHECK_EQ(executor.held_memory(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), 3u);
  CHECK_STATUS(kmt.escape(adapter, connection.device(), transport.context(), bytes, sizeof bytes),
               Status::invalid_parameter);
  transport.teardown();
  connection.close();
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.handles().outstanding(), 0u); }

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
  CHECK_EQ(kmt.fences().completed(), completed + (doomed.submissions_through_kernel() - through_kernel));
  CHECK_EQ(kmt.last_code(), 0u);
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
    CHECK(rig.kmt().open_objects() >= 4u);
    rig.kmt().end_process();
    CHECK_EQ(rig.kmt().open_objects(), 0u);
    CHECK_EQ(rig.device().device().contexts(), 0u);
    CHECK_EQ(rig.platform().mapped_frames(), 0u);
    CHECK_EQ(rig.platform().unclaimed_violations(), 0u);
    connection.close(); } }
