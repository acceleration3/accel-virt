/* One instance's connection over MockKmt: it opens on the rvkgpu adapter among
 * others with that adapter's LUID and a Vulkan context whose ring is order 20,
 * staging its commands in 256 KiB it takes when it opens, sends one command at a
 * time under its lock, answers not_supported for every reason it cannot open
 * and no_memory when the staging cannot be had, with nothing left open, stays
 * faulted once the host fails it, and closes everything it opened. Invariants
 * after every step. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/context/rvk_context_record.hpp"
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

/* A view held of a reply slot the transport no longer names. */
struct KmtTransportProbe {
    static std::uint32_t forget_reply(KmtTransport &t)
    {
        const std::uint32_t reply = t.allocations_[2];
        t.allocations_[2] = 0u;
        return reply;
    }
    static void restore_reply(KmtTransport &t, std::uint32_t reply) { t.allocations_[2] = reply; }
};

}  // namespace av

namespace av::icd {

struct ConnectionProbe {
    static GuestConnection &guest(Connection &c) { return c.connection_; }
    static KmtTransport    &transport(Connection &c) { return c.transport_.object(); }
    static AdapterSearch   &search(Connection &c) { return c.search_; }
    static std::uint8_t    *swap_staging(Connection &c, std::uint8_t *staging)
    {
        std::uint8_t *const kept = c.staging_;
        c.staging_ = staging;
        return kept;
    }
    /* A transport built while the connection is closed, or taken away while it is open. */
    static void build_transport(Connection &c)
    {
        CHECK_STATUS(c.transport_.construct(c.connection_, SubApi::vulkan, Connection::ring_order,
                                            Connection::reply_bytes, nullptr, 0u),
                     Status::ok);
    }
    static void drop_transport(Connection &c) { c.transport_.destroy(); }
};

}  // namespace av::icd

using av::GuestConnectionProbe;
using av::KmtTransportProbe;
using av::icd::ConnectionProbe;

namespace {

const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
/* The rvkgpu adapter is the second one added. */
constexpr std::uint32_t rvkgpu_index = 1u;
/* What an open connection holds: the adapter, a device, its paging queue, the
 * transport's four allocations, its DMA mapping and its context. */
constexpr std::uint32_t open_objects = 9u;

class Rig {
public:
    Rig() : lock_(trace_, 'S'), connection_(kmt_, platform_, lock_)
    {
        kmt_.add_adapter(basic_render, 3100u);
        kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
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
    Connection          connection_;
};

/* vkCreateInstance, which names no dispatchable handle to unwrap, through the
 * connection's transport. */
VkResult create_instance(Connection &connection, std::uint64_t &instance)
{
    VkInstanceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    return av::vkproto::rvk_vkCreateInstance(connection.transport(), &create, nullptr, &instance);
}

}  // namespace

TEST(connection, is_closed_until_opened)
{ Rig rig;
  rig.check_nothing_open(); }

TEST(connection, opens_a_vulkan_context_on_the_rvkgpu_adapter_with_its_luid)
{ Rig rig;
  rig.kmt().set_adapter_luid(0u, 0x0000000500000011ull);
  rig.kmt().set_adapter_luid(rvkgpu_index, 0x0000000700000042ull);
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().opened());
  CHECK(!rig.connection().faulted());
  /* The LUID of the adapter the connection kept, not the other one's. */
  CHECK_EQ(rig.connection().luid(), 0x0000000700000042ull);
  CHECK_EQ(ConnectionProbe::guest(rig.connection()).adapter(), ConnectionProbe::search(rig.connection()).adapter());
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  CHECK_EQ(rig.platform().live_allocations(), 1u);
  av::RvkContextRecord record;
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
      bool          no_rvkgpu;
  };
  const Refusal refusals[] = {
      {"no rvkgpu adapter", MockKmt::Call::count, true},
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
      Connection          connection(kmt, platform, lock);
      kmt.add_adapter(basic_render, 3100u);
      if (!refusal.no_rvkgpu) kmt.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
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
  std::uint8_t     stray = 0u;
  /* Closed, with staging kept, a transport built, an adapter still kept, a device still open. */
  CHECK(ConnectionProbe::swap_staging(connection, &stray) == nullptr);
  CHECK(!connection.invariants_hold());
  CHECK(ConnectionProbe::swap_staging(connection, nullptr) == &stray);
  CHECK(connection.invariants_hold());
  ConnectionProbe::build_transport(connection);
  CHECK(!connection.invariants_hold());
  ConnectionProbe::drop_transport(connection);
  CHECK(connection.invariants_hold());
  CHECK_STATUS(search.find(rvkgpu), Status::ok);
  CHECK(!connection.invariants_hold());
  CHECK_STATUS(guest.open(search.adapter()), Status::ok);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK(!connection.invariants_hold());
  guest.close();
  CHECK(connection.invariants_hold());
  /* Open, with a view of a reply slot it does not name, no staging, the device
   * gone, a device without its queue, another adapter. */
  CHECK_STATUS(connection.open(), Status::ok);
  KmtTransport &transport = ConnectionProbe::transport(connection);
  const std::uint32_t reply = KmtTransportProbe::forget_reply(transport);
  CHECK(!connection.invariants_hold());
  KmtTransportProbe::restore_reply(transport, reply);
  CHECK(connection.invariants_hold());
  std::uint8_t *const staging = ConnectionProbe::swap_staging(connection, nullptr);
  CHECK(!connection.invariants_hold());
  ConnectionProbe::swap_staging(connection, staging);
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
  /* Last, since neither the adapter nor the transport can be given back. */
  ConnectionProbe::drop_transport(connection);
  CHECK(!connection.invariants_hold());
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
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().command_id(0u), static_cast<std::uint32_t>(av::vkproto::kCmd_vkCmdDraw));
  CHECK_EQ(rig.kmt().command_size(0u), 16u);
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
