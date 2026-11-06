#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "core/support/object_storage.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_allocation.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/host_channel_probe.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::GuestConnection;
using av::KmtTransport;
using av::LockTrace;
using av::MockKmt;
using av::MockPlatform;
using av::PciIdentity;
using av::RecordingLog;
using av::RecordingThreadLock;
using av::ScriptedHost;
using av::Status;
using av::icd::Connection;

namespace av {

struct GuestConnectionProbe {
    static void set_device(GuestConnection &c, std::uint32_t device) { c.device_ = device; }
    static void set_queue(GuestConnection &c, std::uint32_t queue) { c.queue_ = queue; }
    static void set_adapter(GuestConnection &c, std::uint32_t adapter) { c.adapter_ = adapter; }
};

struct KmtTransportProbe {
    static std::uint32_t forget_name(KmtTransport &t)
    {
        const std::uint32_t name = t.allocations_[0];
        t.allocations_[0] = 0u;
        return name;
    }
    static void restore_name(KmtTransport &t, std::uint32_t name) { t.allocations_[0] = name; }
};

}

namespace av::icd {

struct LossLatchProbe {
    static void forget_the_lost_bit(LossLatch &latch) { latch.word_ = static_cast<std::uint32_t>(FaultCode::device_lost); }
};

struct OpenedObjectsProbe {
    static void add_a_nameless_row(OpenedObjects &opened)
    {
        std::uint32_t slot = 0u;
        CHECK_STATUS(opened.opens_[0].insert(0u, OpenedObjectRow{0u, 0u, 0u}, slot), Status::ok);
    }
    static void clear(OpenedObjects &opened) { opened.opens_[0].clear(); }
};

struct ConnectionProbe {
    static LossLatch &loss(Connection &c) { return c.loss_; }
    static void       set_opened(Connection &c, bool opened) { c.opened_ = opened; }
    static GuestConnection &guest(Connection &c) { return c.connection_; }
    static HostChannel     &channel(Connection &c) { return c.channel_; }
    static AdapterSearch   &search(Connection &c) { return c.search_; }
};

}

using av::GuestConnectionProbe;
using av::KmtTransportProbe;
using av::icd::ConnectionProbe;
using av::icd::LossLatchProbe;
using av::icd::OpenedObjectsProbe;
using av::icd::HostChannelProbe;

namespace {

const PciIdentity avgpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
constexpr std::uint32_t avgpu_index = 1u;
constexpr std::uint32_t open_objects = 7u;

class Rig {
public:
    Rig()
        : lock_(trace_, 'S'), log_(), lines_(platform_, log_, "avtest: "), handles_(kmt_, lock_, platform_),
          connection_(kmt_, platform_, lock_, lines_, handles_)
    {
        kmt_.add_adapter(basic_render, 3100u);
        kmt_.add_adapter(avgpu, AdapterScenario::wddm_3_2);
    }

    MockKmt             &kmt() { return kmt_; }
    MockPlatform        &platform() { return platform_; }
    LockTrace           &trace() { return trace_; }
    RecordingThreadLock &lock() { return lock_; }
    Connection          &connection() { return connection_; }

    void check_nothing_open()
    {
        CHECK(!connection_.opened());
        CHECK(!connection_.lost());
        CHECK_EQ(connection_.luid(), 0u);
        CHECK_EQ(connection_.adapter(), 0u);
        CHECK_EQ(connection_.device(), 0u);
        CHECK_EQ(connection_.submissions(), 0u);
        CHECK_EQ(kmt_.open_objects(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_EQ(platform_.live_allocations(), 0u);
        CHECK(connection_.invariants_hold());
    }

private:
    MockKmt             kmt_;
    MockPlatform        platform_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    RecordingLog        log_;
    av::KeyedLog        lines_;
    av::icd::SharedHandles handles_;
    Connection          connection_;
};

VkResult create_instance(Connection &connection, std::uint64_t &instance)
{
    av::vkproto::HandleIdentity identity;
    VkInstanceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    return av::vkproto::av_vkCreateInstance(connection.transport(), identity, &create, nullptr, &instance);
}

}

TEST(connection, is_closed_until_opened)
{ Rig rig;
  rig.check_nothing_open(); }

TEST(connection, opens_a_vulkan_context_on_the_avgpu_adapter_with_its_luid)
{ Rig rig;
  rig.kmt().set_adapter_luid(0u, 0x0000000500000011ull);
  rig.kmt().set_adapter_luid(avgpu_index, 0x0000000700000042ull);
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().opened());
  CHECK(!rig.connection().lost());
  CHECK_EQ(rig.connection().luid(), 0x0000000700000042ull);
  CHECK_EQ(ConnectionProbe::guest(rig.connection()).adapter(), ConnectionProbe::search(rig.connection()).adapter());
  CHECK_EQ(rig.connection().device(), ConnectionProbe::guest(rig.connection()).device());
  CHECK(rig.connection().device() != 0u);
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK_EQ(rig.platform().live_allocations(), 1u);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::vulkan);
  CHECK_EQ(record.ring_order(), 20u);
  CHECK(!rig.lock().held()); }

TEST(connection, a_command_crosses_under_the_lock_and_its_answer_comes_back)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  ScriptedHost host(rig.kmt());
  host.vk_handle(0, ScriptedHost::vk_instance);
  std::uint64_t instance = 0u;
  CHECK_EQ(create_instance(rig.connection(), instance), VK_SUCCESS);
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(instance, ScriptedHost::vk_instance);
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().command_id(0u), static_cast<std::uint32_t>(av::vkproto::kCmd_vkCreateInstance));
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0);
  CHECK(!rig.connection().lost()); }

TEST(connection, lends_a_command_all_of_its_staging_and_spills_past_it)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  av::WireWriter writer = rig.connection().transport().request_writer();
  writer.write_zeros(Connection::command_bytes - 8u);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK(!writer.spilled());
  writer.write_u8(0u);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK(writer.spilled());
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK(rig.connection().transport().send_async(av::vkproto::kCmd_vkCreateInstance, writer) ==
        av::TransportResult::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 2u);
  CHECK(!rig.lock().held());
  CHECK(!rig.connection().lost());
  CHECK(rig.connection().invariants_hold()); }

TEST(connection, answers_not_supported_for_every_reason_it_cannot_open_and_leaves_nothing_open)
{ struct Refusal {
      const char   *why;
      MockKmt::Call call;
      bool          no_avgpu;
  };
  const Refusal refusals[] = {
      {"no avgpu adapter", MockKmt::Call::count, true},
      {"adapters cannot be enumerated", MockKmt::Call::enumerate_adapters, false},
      {"no LUID for the adapter", MockKmt::Call::adapter_luid, false},
      {"no device", MockKmt::Call::create_device, false},
      {"no paging queue", MockKmt::Call::create_paging_queue, false},
      {"no ring", MockKmt::Call::create_allocation, false},
      {"a capset without Vulkan", MockKmt::Call::create_context_virtual, false},
  };
  for (const Refusal &refusal : refusals) {
      MockKmt             kmt;
      MockPlatform        platform;
      LockTrace           trace;
      RecordingThreadLock lock(trace, 'S');
      RecordingLog        log;
      av::KeyedLog        lines(platform, log, "avtest: ");
      av::icd::SharedHandles handles(kmt, lock, platform);
      Connection          connection(kmt, platform, lock, lines, handles);
      kmt.add_adapter(basic_render, 3100u);
      if (!refusal.no_avgpu) kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
      const std::uint32_t nth = refusal.call == MockKmt::Call::adapter_luid ? 2u : 1u;
      if (refusal.call != MockKmt::Call::count) kmt.fail_at(refusal.call, nth, MockKmt::invalid_parameter);
      const Status status = connection.open(0u);
      if (status != Status::not_supported)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "%s: status %u", refusal.why,
                                                       static_cast<unsigned>(status));
      CHECK(connection.invariants_hold());
      CHECK(!connection.opened());
      CHECK_EQ(connection.luid(), 0u);
      CHECK_EQ(kmt.open_objects(), 0u);
      CHECK_EQ(kmt.misuses(), 0u);
      CHECK_EQ(platform.live_allocations(), 0u);
  }
}

TEST(connection, answers_no_memory_when_its_staging_cannot_be_had_and_leaves_nothing_open)
{ Rig rig;
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(rig.connection().open(0u), Status::no_memory);
  rig.check_nothing_open();
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 0u);
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  CHECK(rig.connection().invariants_hold()); }

TEST(connection, opening_twice_is_refused_and_keeps_the_first)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  CHECK_STATUS(rig.connection().open(0u), Status::invalid_state);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().opened());
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK_EQ(rig.platform().live_allocations(), 1u); }

TEST(connection, closes_everything_it_opened_and_opens_again)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  rig.connection().close();
  rig.check_nothing_open();
  rig.connection().close();
  rig.check_nothing_open();
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), open_objects); }

TEST(connection, a_host_failure_faults_it_and_later_sends_fail_without_a_submission)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  rig.kmt().withhold_answer_at(1u);
  std::uint64_t instance = 0u;
  CHECK_EQ(create_instance(rig.connection(), instance), VK_ERROR_DEVICE_LOST);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().lost());
  CHECK(rig.connection().opened());
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK_EQ(create_instance(rig.connection(), instance), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  CHECK(!rig.lock().held());
  rig.connection().close();
  rig.check_nothing_open(); }

TEST(connection, invariants_see_a_half_open_connection)
{ Rig rig;
  Connection      &connection = rig.connection();
  GuestConnection &guest = ConnectionProbe::guest(connection);
  AdapterSearch   &search = ConnectionProbe::search(connection);
  HostChannelProbe channel(ConnectionProbe::channel(connection));
  std::uint8_t     stray = 0u;
  CHECK(channel.swap_staging(&stray) == nullptr);
  CHECK(!connection.invariants_hold());
  channel.build_transport(guest);
  CHECK(!connection.invariants_hold());
  channel.drop_transport();
  CHECK(channel.swap_staging(nullptr) == &stray);
  CHECK(connection.invariants_hold());
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  CHECK(!connection.invariants_hold());
  CHECK_STATUS(guest.open(search.adapter()), Status::ok);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK(!connection.invariants_hold());
  guest.close();
  CHECK(connection.invariants_hold());
  CHECK_STATUS(connection.open(0u), Status::ok);
  KmtTransport &transport = channel.channel.transport();
  const std::uint32_t name = KmtTransportProbe::forget_name(transport);
  CHECK(!connection.invariants_hold());
  KmtTransportProbe::restore_name(transport, name);
  CHECK(connection.invariants_hold());
  std::uint8_t *const staging = channel.swap_staging(nullptr);
  CHECK(!connection.invariants_hold());
  channel.swap_staging(staging);
  const std::uint32_t device = guest.device();
  GuestConnectionProbe::set_device(guest, 0u);
  CHECK(!connection.invariants_hold());
  GuestConnectionProbe::set_device(guest, device);
  const std::uint32_t queue = guest.queue();
  GuestConnectionProbe::set_queue(guest, 0u);
  CHECK(!connection.invariants_hold());
  GuestConnectionProbe::set_queue(guest, queue);
  const std::uint32_t adapter = guest.adapter();
  GuestConnectionProbe::set_adapter(guest, adapter + 1u);
  CHECK(!connection.invariants_hold());
  GuestConnectionProbe::set_adapter(guest, adapter);
  CHECK(connection.invariants_hold());
  CHECK_STATUS(search.close(), Status::ok);
  CHECK(!connection.invariants_hold());
  channel.drop_transport();
  std::uint8_t *const kept = channel.swap_staging(nullptr);
  CHECK(!connection.invariants_hold());
  channel.swap_staging(kept);
  connection.close();
  CHECK(connection.invariants_hold());
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

namespace {
std::size_t frame(std::uint8_t *bytes, std::uint32_t id, std::uint32_t count)
{
    for (std::uint32_t index = 0u; index < count; index++)
        CHECK_STATUS(av::CommandHeader(id + index, 16u, 0u).encode(bytes + 16u * index, 8u), Status::ok);
    return 16u * count;
}

}

TEST(connection, a_batch_of_framed_commands_is_one_submission_under_the_lock)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  std::uint8_t framed[48] = {};
  const std::size_t length = frame(framed, av::vkproto::kCmd_vkCmdDraw, 3u);
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK(rig.connection().send_batch(framed, length) == av::TransportResult::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), 0u);
  CHECK_EQ(rig.kmt().commands(), 3u);
  for (std::uint32_t index = 0u; index < 3u; index++) {
      CHECK_EQ(rig.kmt().command_id(index), static_cast<std::uint32_t>(av::vkproto::kCmd_vkCmdDraw) + index);
      CHECK_EQ(rig.kmt().command_size(index), 16u);
  }
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0); }

TEST(connection, a_batch_that_does_not_frame_is_refused_before_it_is_published)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  std::uint8_t framed[48] = {};
  const std::size_t length = frame(framed, av::vkproto::kCmd_vkCmdDraw, 3u);
  CHECK(rig.connection().send_batch(framed, length - 1u) == av::TransportResult::failed);
  CHECK(rig.connection().send_batch(framed, 0u) == av::TransportResult::failed);
  CHECK(rig.connection().invariants_hold());
  CHECK(!rig.connection().lost());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(!rig.lock().held());
  CHECK(std::strcmp(rig.trace().text(), "SsSs") == 0); }

namespace {

constexpr std::uint64_t memory_bytes = std::uint64_t{64} << 20u;
constexpr std::uint64_t first_memory = 0x0000700000000001ull;
constexpr std::uint64_t second_memory = 0x0000700000000002ull;
const av::HostMemoryRequest vulkan_type(av::HostMemoryRequest::Kind::vulkan, 3u);
const int                   owner_token = 0;
const void *const           owner = &owner_token;

}

TEST(connection_memory, opens_finds_and_closes_host_memory_under_its_lock)
{ Rig rig;
  rig.kmt().open_host_window(2u * memory_bytes);
  ScriptedHost host(rig.kmt());
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  const std::uint32_t allocations = rig.kmt().calls(MockKmt::Call::create_allocation);
  av::HostAllocation *made = nullptr;
  host.blob_import(0u, first_memory);
  CHECK_STATUS(rig.connection().channel().open_memory(memory_bytes, vulkan_type, owner, made), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK(made != nullptr);
  CHECK(made->invariants_hold());
  CHECK_EQ(made->handle(), first_memory);
  CHECK_EQ(made->bytes(), memory_bytes);
  CHECK(made->view() == rig.kmt().window_base());
  CHECK_EQ(rig.kmt().window_request(MockKmt::first_window_resource).encoded(), vulkan_type.encoded());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), allocations);
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0);
  CHECK(!rig.lock().held());
  CHECK(rig.connection().channel().find_memory(first_memory, owner) == made);
  CHECK(rig.connection().channel().find_memory(second_memory, owner) == nullptr);
  CHECK(std::strcmp(rig.trace().text(), "SsSsSs") == 0);
  host.nop();
  CHECK_STATUS(rig.connection().channel().close_memory(*made), Status::ok);
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK(rig.connection().channel().find_memory(first_memory, owner) == nullptr);
  CHECK(std::strcmp(rig.trace().text(), "SsSsSsSsSs") == 0);
  CHECK_EQ(rig.kmt().misuses(), 0u);
  CHECK(!rig.lock().held()); }

TEST(connection_memory, memory_refused_leaves_nothing)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  av::HostAllocation *made = nullptr;
  CHECK_STATUS(rig.connection().channel().open_memory(memory_bytes, vulkan_type, owner, made), Status::not_supported);
  CHECK(made == nullptr);
  rig.kmt().open_host_window(memory_bytes);
  ScriptedHost host(rig.kmt());
  host.blob_import(av::BlobImportAnswer::executor_refused, 0u);
  CHECK_STATUS(rig.connection().channel().open_memory(memory_bytes, vulkan_type, owner, made), Status::invalid_parameter);
  CHECK(made == nullptr);
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK_EQ(rig.kmt().misuses(), 0u);
  CHECK(!rig.lock().held()); }

TEST(connection_memory, makes_no_memory_closed_or_faulted_and_abandons_what_a_faulted_one_made)
{ Rig rig;
  rig.kmt().open_host_window(2u * memory_bytes);
  ScriptedHost host(rig.kmt());
  av::HostAllocation *made = reinterpret_cast<av::HostAllocation *>(&rig);
  CHECK_STATUS(rig.connection().channel().open_memory(memory_bytes, vulkan_type, owner, made), Status::invalid_state);
  CHECK(made == nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 0u);
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0);

  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  host.blob_import(0u, first_memory);
  CHECK_STATUS(rig.connection().channel().open_memory(memory_bytes, vulkan_type, owner, made), Status::ok);
  const std::uint32_t asked = rig.kmt().calls(MockKmt::Call::escape);
  rig.kmt().withhold_answer_at(1u);
  std::uint64_t instance = 0u;
  CHECK_EQ(create_instance(rig.connection(), instance), VK_ERROR_DEVICE_LOST);
  av::HostAllocation *other = nullptr;
  CHECK_STATUS(rig.connection().channel().open_memory(memory_bytes, vulkan_type, owner, other), Status::invalid_state);
  CHECK(other == nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), asked);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_STATUS(rig.connection().channel().close_memory(*made), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), asked);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  rig.connection().close();
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK(!rig.lock().held()); }

TEST(connection_memory, memory_whose_release_is_refused_is_still_let_go)
{ Rig rig;
  rig.kmt().open_host_window(memory_bytes);
  ScriptedHost host(rig.kmt());
  CHECK_STATUS(rig.connection().open(0u), Status::ok);
  av::HostAllocation *made = nullptr;
  host.blob_import(0u, first_memory);
  CHECK_STATUS(rig.connection().channel().open_memory(memory_bytes, vulkan_type, owner, made), Status::ok);
  host.nop();
  rig.kmt().fail_at(MockKmt::Call::escape, 1u, MockKmt::invalid_parameter);
  CHECK(rig.connection().channel().close_memory(*made) != Status::ok);
  CHECK(rig.connection().channel().find_memory(first_memory, owner) == nullptr);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK(!rig.lock().held());
  rig.connection().close();
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }

TEST(connection, remembers_whether_the_host_shares_by_dma_buf_and_starts_saying_no)
{ Rig rig;
  CHECK(!rig.connection().host_has_dma_buf());
  CHECK(rig.connection().invariants_hold());
  rig.connection().note_host_dma_buf();
  CHECK(rig.connection().host_has_dma_buf());
  rig.connection().note_host_dma_buf();
  CHECK(rig.connection().host_has_dma_buf());
  CHECK(rig.connection().invariants_hold()); }

TEST(connection, learns_what_windows_calls_its_adapter)
{ { Rig rig;
    rig.kmt().describe_adapter(avgpu_index, av::PciLocation(0x7Bu, 0u, 0u), "AMD Radeon(TM) Graphics", 0u);
    CHECK(std::strcmp(rig.connection().adapter_name(), "") == 0);
    CHECK_STATUS(rig.connection().open(0u), Status::ok);
    CHECK(std::strcmp(rig.connection().adapter_name(), "AMD Radeon(TM) Graphics") == 0);
    rig.connection().close();
    CHECK(std::strcmp(rig.connection().adapter_name(), "") == 0); }
  { Rig rig;
    rig.kmt().describe_adapter(avgpu_index, av::PciLocation(0x7Bu, 0u, 0u), "AMD Radeon(TM) Graphics", 0u);
    rig.kmt().fail_at(MockKmt::Call::query_adapter_name, 1u, MockKmt::invalid_parameter);
    CHECK_STATUS(rig.connection().open(0u), Status::ok);
    CHECK(std::strcmp(rig.connection().adapter_name(), "") == 0); } }

TEST(connection, invariants_see_a_connection_whose_rows_or_latch_are_unsound)
{ Rig rig;
  Connection &connection = rig.connection();
  OpenedObjectsProbe::add_a_nameless_row(connection.opened_objects());
  CHECK(!connection.invariants_hold());
  OpenedObjectsProbe::clear(connection.opened_objects());
  CHECK(connection.invariants_hold());
  ConnectionProbe::loss(connection).lose(av::FaultCode::device_lost);
  LossLatchProbe::forget_the_lost_bit(ConnectionProbe::loss(connection));
  CHECK(!connection.invariants_hold());
  ConnectionProbe::loss(connection).forget();
  CHECK(connection.invariants_hold()); }

TEST(connection, invariants_see_a_lost_latch_beside_a_channel_that_is_not_and_a_flag_that_disagrees_with_the_channel)
{ Rig rig;
  Connection &connection = rig.connection();
  ConnectionProbe::loss(connection).lose(av::FaultCode::device_lost);
  CHECK(connection.invariants_hold());
  ConnectionProbe::loss(connection).forget();
  CHECK_STATUS(connection.open(0u), Status::ok);
  CHECK(connection.invariants_hold());
  ConnectionProbe::loss(connection).lose(av::FaultCode::device_lost);
  CHECK(!connection.invariants_hold());
  ConnectionProbe::loss(connection).forget();
  CHECK(connection.invariants_hold());
  ConnectionProbe::set_opened(connection, false);
  CHECK(!connection.invariants_hold());
  ConnectionProbe::set_opened(connection, true);
  CHECK(connection.invariants_hold());
  connection.close();
  CHECK(connection.invariants_hold());
  ConnectionProbe::set_opened(connection, true);
  CHECK(!connection.invariants_hold());
  ConnectionProbe::set_opened(connection, false);
  CHECK(connection.invariants_hold()); }
