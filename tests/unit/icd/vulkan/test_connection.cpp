/* One instance's connection over MockKmt: it opens on the avgpu adapter among
 * others with that adapter's LUID and a Vulkan context whose ring is order 20,
 * staging its commands in 256 KiB it takes when it opens, sends one command at a
 * time under its lock, answers not_supported for every reason it cannot open
 * and no_memory when the staging cannot be had, with nothing left open, stays
 * faulted once the host fails it, and closes everything it opened. Invariants
 * after every step. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "core/support/object_storage.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_window_chunk.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/host_channel_probe.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
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

/* An established context whose name the transport no longer holds. */
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

namespace av::icd {

struct ConnectionProbe {
    static GuestConnection &guest(Connection &c) { return c.connection_; }
    static HostChannel     &channel(Connection &c) { return c.channel_; }
    static AdapterSearch   &search(Connection &c) { return c.search_; }
};

}  // namespace av::icd

using av::GuestConnectionProbe;
using av::KmtTransportProbe;
using av::icd::ConnectionProbe;
using av::icd::HostChannelProbe;

namespace {

const PciIdentity avgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::av_subsystem);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
/* The avgpu adapter is the second one added. */
constexpr std::uint32_t avgpu_index = 1u;
/* What an open connection holds: the adapter, a device, its paging queue, the
 * transport's two allocations, its DMA mapping and its context. */
constexpr std::uint32_t open_objects = 7u;

class Rig {
public:
    Rig() : lock_(trace_, 'S'), log_(), connection_(kmt_, platform_, lock_, log_)
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
        CHECK(!connection_.faulted());
        CHECK_EQ(connection_.luid(), 0u);
        CHECK_EQ(connection_.adapter(), 0u);
        /* The D3DKMT device a swapchain's pacing waits on: none while closed. */
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
    Connection          connection_;
};

/* vkCreateInstance, which names no dispatchable handle to unwrap, through the
 * connection's transport. */
VkResult create_instance(Connection &connection, std::uint64_t &instance)
{
    VkInstanceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    return av::vkproto::av_vkCreateInstance(connection.transport(), &create, nullptr, &instance);
}

}  // namespace

TEST(connection, is_closed_until_opened)
{ Rig rig;
  rig.check_nothing_open(); }

TEST(connection, opens_a_vulkan_context_on_the_avgpu_adapter_with_its_luid)
{ Rig rig;
  rig.kmt().set_adapter_luid(0u, 0x0000000500000011ull);
  rig.kmt().set_adapter_luid(avgpu_index, 0x0000000700000042ull);
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().opened());
  CHECK(!rig.connection().faulted());
  /* The LUID of the adapter the connection kept, not the other one's. */
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
  CHECK_STATUS(rig.connection().open(), Status::ok);
  ScriptedHost host(rig.kmt());
  host.vk_handle(0, ScriptedHost::vk_instance);
  std::uint64_t instance = 0u;
  CHECK_EQ(create_instance(rig.connection(), instance), VK_SUCCESS);
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(instance, ScriptedHost::vk_instance);
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().command_id(0u), static_cast<std::uint32_t>(av::vkproto::kCmd_vkCreateInstance));
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0);
  CHECK(!rig.connection().faulted()); }

/* A command as large as the staging: the room it lends is the 256 KiB less the
 * command's header. */
TEST(connection, lends_a_command_all_of_its_staging)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  av::WireWriter writer = rig.connection().transport().request_writer();
  writer.write_zeros(Connection::command_bytes - 8u);
  CHECK_STATUS(writer.status(), Status::ok);
  writer.write_u8(0u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(rig.connection().transport().send_async(av::vkproto::kCmd_vkCreateInstance, writer) ==
        av::TransportResult::failed);
  CHECK(!rig.lock().held());
  CHECK(rig.connection().invariants_hold()); }

TEST(connection, answers_not_supported_for_every_reason_it_cannot_open_and_leaves_nothing_open)
{ struct Refusal {
      const char   *why;
      MockKmt::Call call;      /* the call that fails; Call::count for none */
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
      Connection          connection(kmt, platform, lock, log);
      kmt.add_adapter(basic_render, 3100u);
      if (!refusal.no_avgpu) kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
      if (refusal.call != MockKmt::Call::count) kmt.fail_at(refusal.call, 1u, MockKmt::invalid_parameter);
      const Status status = connection.open();
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
  CHECK_STATUS(rig.connection().open(), Status::no_memory);
  rig.check_nothing_open();
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 0u);
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK(rig.connection().invariants_hold()); }

TEST(connection, opening_twice_is_refused_and_keeps_the_first)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK_STATUS(rig.connection().open(), Status::invalid_state);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().opened());
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK_EQ(rig.platform().live_allocations(), 1u); }

TEST(connection, closes_everything_it_opened_and_opens_again)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  rig.connection().close();
  rig.check_nothing_open();
  rig.connection().close();
  rig.check_nothing_open();
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), open_objects); }

TEST(connection, a_host_failure_faults_it_and_later_sends_fail_without_a_submission)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  rig.kmt().withhold_answer_at(1u);
  std::uint64_t instance = 0u;
  CHECK_EQ(create_instance(rig.connection(), instance), VK_ERROR_DEVICE_LOST);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().faulted());
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
  /* Closed, with staging kept, a channel open under it, an adapter still
   * kept, a device still open. */
  CHECK(channel.swap_staging(&stray) == nullptr);
  CHECK(!connection.invariants_hold());
  channel.build_transport(guest);
  CHECK(!connection.invariants_hold());
  channel.drop_transport();
  CHECK(channel.swap_staging(nullptr) == &stray);
  CHECK(connection.invariants_hold());
  CHECK_STATUS(search.find(avgpu), Status::ok);
  CHECK(!connection.invariants_hold());
  CHECK_STATUS(guest.open(search.adapter()), Status::ok);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK(!connection.invariants_hold());
  guest.close();
  CHECK(connection.invariants_hold());
  /* Open, with a context it does not name, no staging, the device gone, a
   * device without its queue, another adapter. */
  CHECK_STATUS(connection.open(), Status::ok);
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
  /* Last, since neither the adapter nor the transport can be given back: a
   * channel closed under the lock that still sends through it. */
  channel.drop_transport();
  std::uint8_t *const kept = channel.swap_staging(nullptr);
  CHECK(!connection.invariants_hold());
  channel.swap_staging(kept);
  connection.close();
  CHECK(connection.invariants_hold());
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

namespace {

/* `count` framed commands of eight bytes of body each, numbered from `id`. */
std::size_t frame(std::uint8_t *bytes, std::uint32_t id, std::uint32_t count)
{
    for (std::uint32_t index = 0u; index < count; index++)
        CHECK_STATUS(av::CommandHeader(id + index, 16u, 0u).encode(bytes + 16u * index, 8u), Status::ok);
    return 16u * count;
}

}  // namespace

TEST(connection, a_batch_of_framed_commands_is_one_submission_under_the_lock)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  std::uint8_t framed[48] = {};
  const std::size_t length = frame(framed, av::vkproto::kCmd_vkCmdDraw, 3u);
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK(rig.connection().send_batch(framed, length) == av::TransportResult::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), 0u);
  /* One submission, and the device finds every command framed in it. */
  CHECK_EQ(rig.kmt().commands(), 3u);
  for (std::uint32_t index = 0u; index < 3u; index++) {
      CHECK_EQ(rig.kmt().command_id(index), static_cast<std::uint32_t>(av::vkproto::kCmd_vkCmdDraw) + index);
      CHECK_EQ(rig.kmt().command_size(index), 16u);
  }
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0); }

TEST(connection, a_batch_that_does_not_frame_is_refused_before_it_is_published)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  std::uint8_t framed[48] = {};
  const std::size_t length = frame(framed, av::vkproto::kCmd_vkCmdDraw, 3u);
  CHECK(rig.connection().send_batch(framed, length - 1u) == av::TransportResult::failed);
  CHECK(rig.connection().send_batch(framed, 0u) == av::TransportResult::failed);
  CHECK(rig.connection().invariants_hold());
  CHECK(!rig.connection().faulted());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(!rig.lock().held());
  CHECK(std::strcmp(rig.trace().text(), "SsSs") == 0); }

namespace {

constexpr std::uint64_t chunk_bytes = std::uint64_t{64} << 20u;

}  // namespace

/* A chunk is asked of the kernel on the connection's context, under the
 * connection's lock - guest RAM, with no window here - and closing it gives it
 * back. */
TEST(connection, opens_a_chunk_under_its_lock_and_closes_it)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK_EQ(rig.connection().adapter(), ConnectionProbe::search(rig.connection()).adapter());
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  av::ObjectStorage<av::HostWindowChunk> chunk;
  CHECK_STATUS(rig.connection().open_chunk(chunk, chunk_bytes), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK(chunk.constructed());
  CHECK(chunk.object().invariants_hold());
  CHECK(chunk.object().view() != nullptr);
  CHECK_EQ(chunk.object().resource_id(), MockKmt::first_chunk_resource);
  CHECK_EQ(chunk.object().bytes(), chunk_bytes);
  CHECK_EQ(rig.kmt().ram_chunks(), 1u);
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0);
  CHECK(!rig.lock().held());

  CHECK_STATUS(rig.connection().close_chunk(chunk), Status::ok);
  CHECK(!chunk.constructed());
  CHECK_EQ(rig.kmt().ram_chunks(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 2u);
  CHECK(std::strcmp(rig.trace().text(), "SsSs") == 0);
  CHECK_EQ(rig.kmt().misuses(), 0u);
  /* Closing what is not there is nothing. */
  CHECK_STATUS(rig.connection().close_chunk(chunk), Status::ok);
  CHECK(std::strcmp(rig.trace().text(), "SsSs") == 0); }

/* A chunk the kernel refuses is the answer, with nothing made. */
TEST(connection, a_chunk_refused_leaves_nothing)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  const std::uint32_t before = rig.kmt().calls(MockKmt::Call::escape);
  rig.kmt().fail_at(MockKmt::Call::escape, 1u, MockKmt::invalid_parameter);
  av::ObjectStorage<av::HostWindowChunk> chunk;
  CHECK(rig.connection().open_chunk(chunk, chunk_bytes) != Status::ok);
  CHECK(!chunk.constructed());
  CHECK(rig.connection().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), before + 1u);
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK_EQ(rig.kmt().ram_chunks(), 0u);
  CHECK_EQ(rig.kmt().misuses(), 0u);
  CHECK(!rig.lock().held()); }

/* A connection not open, or one the host has failed, makes no chunk and asks
 * the Kmt nothing; storage already holding one is not overwritten. */
TEST(connection, makes_no_chunk_closed_faulted_or_over_a_held_one)
{ Rig rig;
  av::ObjectStorage<av::HostWindowChunk> chunk;
  CHECK_STATUS(rig.connection().open_chunk(chunk, chunk_bytes), Status::invalid_state);
  CHECK(!chunk.constructed());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 0u);

  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK_STATUS(rig.connection().open_chunk(chunk, chunk_bytes), Status::ok);
  const std::uint32_t asked = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.connection().open_chunk(chunk, chunk_bytes), Status::invalid_state);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), asked);
  CHECK_EQ(rig.kmt().ram_chunks(), 1u);

  rig.kmt().withhold_answer_at(1u);
  std::uint64_t instance = 0u;
  CHECK_EQ(create_instance(rig.connection(), instance), VK_ERROR_DEVICE_LOST);
  av::ObjectStorage<av::HostWindowChunk> other;
  CHECK_STATUS(rig.connection().open_chunk(other, chunk_bytes), Status::invalid_state);
  CHECK(!other.constructed());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), asked);
  /* A faulted connection lets go of what it made, and leaves the close to the
   * kernel, which closes it with the context. */
  CHECK_STATUS(rig.connection().close_chunk(chunk), Status::ok);
  CHECK(!chunk.constructed());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), asked);
  CHECK_EQ(rig.kmt().ram_chunks(), 1u);
  rig.connection().close();
  CHECK_EQ(rig.kmt().ram_chunks(), 0u);
  CHECK(!rig.lock().held()); }

/* On a device with a host window the chunk is the window's: one escape, no
 * allocation, and one escape to close it, each under the lock. */
TEST(connection, opens_a_window_chunk_under_its_lock_and_closes_it)
{ Rig rig;
  rig.kmt().open_host_window(2u * chunk_bytes);
  CHECK_STATUS(rig.connection().open(), Status::ok);
  const std::uint32_t allocations = rig.kmt().calls(MockKmt::Call::create_allocation);
  av::ObjectStorage<av::HostWindowChunk> chunk;
  CHECK_STATUS(rig.connection().open_chunk(chunk, chunk_bytes), Status::ok);
  CHECK(chunk.object().invariants_hold());
  CHECK_EQ(chunk.object().resource_id(), MockKmt::first_window_resource);
  CHECK_EQ(chunk.object().bytes(), chunk_bytes);
  CHECK(chunk.object().view() == rig.kmt().window_base());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), allocations);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0);
  CHECK_STATUS(rig.connection().close_chunk(chunk), Status::ok);
  CHECK(!chunk.constructed());
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK(std::strcmp(rig.trace().text(), "SsSs") == 0);
  CHECK_EQ(rig.kmt().misuses(), 0u); }

/* The Kmt's refusal to give a chunk back is reported, and the connection lets
 * go of it all the same: the kernel closes it with the context. */
TEST(connection, a_chunk_whose_release_is_refused_is_still_closed)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  av::ObjectStorage<av::HostWindowChunk> chunk;
  CHECK_STATUS(rig.connection().open_chunk(chunk, chunk_bytes), Status::ok);
  rig.kmt().fail_at(MockKmt::Call::escape, 1u, MockKmt::invalid_parameter);
  CHECK(rig.connection().close_chunk(chunk) != Status::ok);
  CHECK(!chunk.constructed());
  CHECK_EQ(rig.kmt().ram_chunks(), 1u);
  CHECK(!rig.lock().held());
  rig.connection().close();
  CHECK_EQ(rig.kmt().ram_chunks(), 0u); }

/* What the host said when it was asked for its device extensions, kept for the
 * one decision that reads it: whether a shared allocation is exported as a
 * dma-buf or as an opaque fd (feature 053, T031). A connection never asked is
 * taken not to have it, which is the answer every host gave before 053 and the
 * one that shares within a process and not across one. */
TEST(connection, remembers_whether_the_host_shares_by_dma_buf_and_starts_saying_no)
{ Rig rig;
  CHECK(!rig.connection().host_has_dma_buf());
  CHECK(rig.connection().invariants_hold());
  rig.connection().note_host_dma_buf();
  CHECK(rig.connection().host_has_dma_buf());
  /* Said twice by two enumerations is still once. */
  rig.connection().note_host_dma_buf();
  CHECK(rig.connection().host_has_dma_buf());
  CHECK(rig.connection().invariants_hold()); }
