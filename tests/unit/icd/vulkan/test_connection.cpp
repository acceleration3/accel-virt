/* One instance's connection over MockKmt: it opens on the rvkgpu adapter among
 * others with the adapter's LUID and a Vulkan context, sends one command at a
 * time under its lock, answers not_supported for every reason it cannot open
 * with nothing left open, stays faulted once the host fails it, and closes
 * everything it opened. Invariants after every step. */
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
#include "harness/icd/scripted_adapter_luids.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::LockTrace;
using av::MockKmt;
using av::PciIdentity;
using av::RecordingThreadLock;
using av::AdapterSearch;
using av::GuestConnection;
using av::ScriptedAdapterLuids;
using av::ScriptedHost;
using av::Status;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::Connection;

namespace av {

struct GuestConnectionProbe {
    static void set_device(GuestConnection &c, std::uint32_t device) { c.device_ = device; }
    static void set_queue(GuestConnection &c, std::uint32_t queue) { c.queue_ = queue; }
    static void set_adapter(GuestConnection &c, std::uint32_t adapter) { c.adapter_ = adapter; }
};

/* A view held of the reply slot of a transport that has none. */
struct KmtTransportProbe {
    static void phantom_view(KmtTransport &t) { t.locked_[2] = true; }
    static void restore(KmtTransport &t) { t.locked_[2] = false; }
};

}  // namespace av

namespace av::icd {

struct ConnectionProbe {
    static GuestConnection &guest(Connection &c) { return c.connection_; }
    static KmtTransport    &transport(Connection &c) { return c.transport_; }
    static AdapterSearch   &search(Connection &c) { return c.search_; }
};

}  // namespace av::icd

using av::GuestConnectionProbe;
using av::KmtTransportProbe;
using av::icd::ConnectionProbe;

namespace {

const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
/* What an open connection holds: the adapter, a device, its paging queue, the
 * transport's four allocations, its DMA mapping and its context. */
constexpr std::uint32_t open_objects = 9u;

class Rig {
public:
    Rig() : lock_(trace_, 'S'), connection_(kmt_, luids_, lock_)
    {
        kmt_.add_adapter(basic_render, 3100u);
        kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
    }

    MockKmt              &kmt() { return kmt_; }
    ScriptedAdapterLuids &luids() { return luids_; }
    LockTrace            &trace() { return trace_; }
    RecordingThreadLock  &lock() { return lock_; }
    Connection           &connection() { return connection_; }

    void check_nothing_open()
    {
        CHECK(!connection_.opened());
        CHECK_EQ(connection_.luid(), 0u);
        CHECK_EQ(kmt_.open_objects(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK(connection_.invariants_hold());
    }

private:
    MockKmt              kmt_;
    ScriptedAdapterLuids luids_;
    LockTrace            trace_;
    RecordingThreadLock  lock_;
    Connection           connection_;
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
  rig.check_nothing_open();
  CHECK(!rig.connection().faulted()); }

TEST(connection, opens_a_vulkan_context_on_the_rvkgpu_adapter_with_its_luid)
{ Rig rig;
  rig.luids().answer(0x0000000700000042ull);
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().opened());
  CHECK(!rig.connection().faulted());
  CHECK_EQ(rig.connection().luid(), 0x0000000700000042ull);
  /* The LUID was asked of the adapter the connection kept, not the other one. */
  CHECK_EQ(rig.luids().asked(), ConnectionProbe::search(rig.connection()).adapter());
  CHECK_EQ(ConnectionProbe::guest(rig.connection()).adapter(), rig.luids().asked());
  CHECK_EQ(rig.kmt().open_objects(), open_objects);
  av::RvkContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::vulkan);
  CHECK_EQ(record.ring_order(), Connection::ring_order);
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

TEST(connection, answers_not_supported_for_every_reason_it_cannot_open_and_leaves_nothing_open)
{ struct Refusal {
      const char   *why;
      MockKmt::Call call;      /* the call that fails; Call::count for none */
      bool          no_rvkgpu;
      bool          no_luid;
  };
  const Refusal refusals[] = {
      {"no rvkgpu adapter", MockKmt::Call::count, true, false},
      {"adapters cannot be enumerated", MockKmt::Call::enumerate_adapters, false, false},
      {"no LUID for the adapter", MockKmt::Call::count, false, true},
      {"no device", MockKmt::Call::create_device, false, false},
      {"no paging queue", MockKmt::Call::create_paging_queue, false, false},
      {"no ring", MockKmt::Call::create_allocation, false, false},
      {"a capset without Vulkan", MockKmt::Call::create_context_virtual, false, false},
  };
  for (const Refusal &refusal : refusals) {
      MockKmt              kmt;
      ScriptedAdapterLuids luids;
      LockTrace            trace;
      RecordingThreadLock  lock(trace, 'S');
      Connection           connection(kmt, luids, lock);
      kmt.add_adapter(basic_render, 3100u);
      if (!refusal.no_rvkgpu) kmt.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
      if (refusal.no_luid) luids.refuse();
      if (refusal.call != MockKmt::Call::count) kmt.fail_at(refusal.call, 1u, MockKmt::invalid_parameter);
      const Status status = connection.open();
      if (status != Status::not_supported) av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "%s: status %u", refusal.why, static_cast<unsigned>(status));
      CHECK(connection.invariants_hold());
      CHECK(!connection.opened());
      CHECK_EQ(connection.luid(), 0u);
      CHECK_EQ(kmt.open_objects(), 0u);
      CHECK_EQ(kmt.misuses(), 0u);
  }
}

TEST(connection, opening_twice_is_refused_and_keeps_the_first)
{ Rig rig;
  CHECK_STATUS(rig.connection().open(), Status::ok);
  CHECK_STATUS(rig.connection().open(), Status::invalid_state);
  CHECK(rig.connection().invariants_hold());
  CHECK(rig.connection().opened());
  CHECK_EQ(rig.kmt().open_objects(), open_objects); }

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

TEST(connection, a_send_on_a_connection_not_open_fails_and_lets_the_lock_go)
{ Rig rig;
  std::uint64_t instance = 7u;
  CHECK_EQ(create_instance(rig.connection(), instance), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(instance, 7u);
  CHECK(!rig.lock().held());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 0u); }

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
  CHECK(!rig.connection().faulted());
  rig.check_nothing_open(); }

TEST(connection, invariants_see_a_half_open_connection)
{ Rig rig;
  Connection &connection = rig.connection();
  GuestConnection &guest = ConnectionProbe::guest(connection);
  AdapterSearch   &search = ConnectionProbe::search(connection);
  /* Closed, with a view held of nothing, an adapter still kept, a device still open. */
  KmtTransportProbe::phantom_view(ConnectionProbe::transport(connection));
  CHECK(!connection.invariants_hold());
  KmtTransportProbe::restore(ConnectionProbe::transport(connection));
  CHECK(connection.invariants_hold());
  CHECK_STATUS(search.find(rvkgpu), Status::ok);
  CHECK(!connection.invariants_hold());
  CHECK_STATUS(guest.open(search.adapter()), Status::ok);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK(!connection.invariants_hold());
  guest.close();
  CHECK(connection.invariants_hold());
  /* Open, with the device gone, a device without its queue, another adapter. */
  CHECK_STATUS(connection.open(), Status::ok);
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
  /* Last, since the adapter it closes cannot be given back. */
  CHECK_STATUS(search.close(), Status::ok);
  CHECK(!connection.invariants_hold());
  connection.close();
  CHECK(connection.invariants_hold()); }
