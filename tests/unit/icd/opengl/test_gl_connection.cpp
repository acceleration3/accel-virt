#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/common/icd_connection.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::GlContextRig;
using av::MockKmt;
using av::Status;
using av::icd::IcdConnection;

namespace av {

struct GuestConnectionProbe {
    static void set_queue(GuestConnection &c, std::uint32_t queue) { c.queue_ = queue; }
    static void set_device(GuestConnection &c, std::uint32_t device) { c.device_ = device; }
    static std::uint32_t device(const GuestConnection &c) { return c.device_; }
    static std::uint32_t queue(const GuestConnection &c) { return c.queue_; }
};

}

namespace av::icd {

struct IcdConnectionProbe {
    static void lose_adapter(av::icd::IcdConnection &connection) { static_cast<void>(connection.search_.close()); }
    static Status find_adapter(av::icd::IcdConnection &connection) { return connection.search_.find_avgpu(connection.luid_); }
};

}

using av::icd::DeviceLink;
using av::icd::IcdConnectionProbe;

TEST(gl_connection, the_first_user_opens_a_device_the_rest_share_it_and_the_last_closes_it)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  CHECK(connection.invariants_hold());
  CHECK_EQ(connection.users(), 0u);
  DeviceLink *first = nullptr;
  CHECK_STATUS(connection.acquire(first), Status::ok);
  CHECK(first != nullptr);
  CHECK(connection.invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), GlContextRig::link_objects);
  CHECK(first->device().opened());
  CHECK_EQ(connection.adapter(), first->device().adapter());
  DeviceLink *second = nullptr;
  CHECK_STATUS(connection.acquire(second), Status::ok);
  CHECK(second == first);
  CHECK_EQ(connection.users(), 2u);
  CHECK_EQ(first->users(), 2u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 1u);
  CHECK(!first->lost());
  connection.release(*first);
  CHECK(first->device().opened());
  CHECK_EQ(first->users(), 1u);
  CHECK(connection.invariants_hold());
  connection.release(*second);
  CHECK(connection.invariants_hold());
  CHECK_EQ(connection.users(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::close_adapter), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_device), 1u); }

TEST(gl_connection, a_lost_device_is_retired_and_the_next_user_opens_a_new_one)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  DeviceLink *old_link = nullptr;
  CHECK_STATUS(connection.acquire(old_link), Status::ok);
  DeviceLink *sharer = nullptr;
  CHECK_STATUS(connection.acquire(sharer), Status::ok);
  CHECK(sharer == old_link);
  old_link->lose();
  CHECK(connection.invariants_hold());
  CHECK(old_link->lost());
  CHECK(old_link->device().opened());
  DeviceLink *fresh = nullptr;
  CHECK_STATUS(connection.acquire(fresh), Status::ok);
  CHECK(fresh != nullptr);
  CHECK(fresh != old_link);
  CHECK(!fresh->lost());
  CHECK(connection.invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 2u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 1u);
  CHECK_EQ(connection.users(), 3u);
  DeviceLink *shared = nullptr;
  CHECK_STATUS(connection.acquire(shared), Status::ok);
  CHECK(shared == fresh);
  connection.release(*shared);
  connection.release(*old_link);
  connection.release(*sharer);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_device), 1u);
  CHECK(fresh->device().opened());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::close_adapter), 0u);
  CHECK(connection.invariants_hold());
  connection.release(*fresh);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(connection.invariants_hold()); }

TEST(gl_connection, lost_and_held_devices_pile_up_each_in_a_row_of_its_own)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  constexpr std::uint32_t many = 8u;
  DeviceLink *links[many] = {};
  for (std::uint32_t at = 0u; at < many; at++) {
    CHECK_STATUS(connection.acquire(links[at]), Status::ok);
    CHECK(links[at] != nullptr);
    for (std::uint32_t earlier = 0u; earlier < at; earlier++) CHECK(links[at] != links[earlier]);
    links[at]->lose();
    CHECK(connection.invariants_hold());
  }
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), many);
  CHECK_EQ(connection.users(), many);
  for (DeviceLink *link : links) connection.release(*link);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(connection.invariants_hold()); }

TEST(gl_connection, a_row_that_cannot_be_had_is_no_memory_and_leaves_the_adapter_closed)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  DeviceLink *link = nullptr;
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(connection.acquire(link), Status::no_memory);
  CHECK(link == nullptr);
  CHECK_EQ(connection.users(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(connection.invariants_hold());
  rig.platform().fail_allocation_at(0u);
  CHECK_STATUS(connection.acquire(link), Status::ok);
  CHECK(link != nullptr);
  CHECK(connection.invariants_hold());
  connection.release(*link);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_connection, no_avgpu_adapter_or_no_device_is_not_supported_with_nothing_open)
{ GlContextRig rig;
  DeviceLink *link = nullptr;
  rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(link), Status::not_supported);
  CHECK(link == nullptr);
  CHECK_EQ(rig.connection().users(), 0u);
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(link), Status::not_supported);
  CHECK(link == nullptr);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.connection().invariants_hold());
  DeviceLink *first = nullptr;
  CHECK_STATUS(rig.connection().acquire(first), Status::ok);
  first->lose();
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(link), Status::not_supported);
  CHECK(link == nullptr);
  CHECK(first->device().opened());
  CHECK_EQ(rig.kmt().open_objects(), GlContextRig::link_objects);
  CHECK(rig.connection().invariants_hold());
  rig.connection().release(*first);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_connection, other_adapters_are_not_the_avgpu_one)
{ av::MockPlatform platform;
  av::MockKmt kmt;
  kmt.add_adapter(av::PciIdentity(0x8086u, 0x46A6u, 0u, 0u), 3200u);
  av::icd::IcdConnection connection(platform, kmt, 0u);
  DeviceLink *link = nullptr;
  CHECK_STATUS(connection.acquire(link), Status::not_supported);
  CHECK(link == nullptr);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(gl_connection, a_connection_destroyed_with_users_closes_what_it_opened)
{ av::MockPlatform platform;
  av::MockKmt kmt;
  kmt.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
  {
    av::icd::IcdConnection connection(platform, kmt, 0u);
    DeviceLink *link = nullptr;
    CHECK_STATUS(connection.acquire(link), Status::ok);
    CHECK_STATUS(connection.acquire(link), Status::ok);
    link->lose();
    CHECK_STATUS(connection.acquire(link), Status::ok);
  }
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(gl_connection, reports_its_users_and_each_link)
{ GlContextRig rig;
  char bytes[256];
  av::StateWriter idle(bytes, sizeof bytes);
  rig.connection().report(idle);
  CHECK(std::strcmp(idle.dump(), "connection/0 users=0 adapter=0 luid=0") == 0);
  DeviceLink *link = nullptr;
  CHECK_STATUS(rig.connection().acquire(link), Status::ok);
  link->lose();
  DeviceLink *fresh = nullptr;
  CHECK_STATUS(rig.connection().acquire(fresh), Status::ok);
  av::StateWriter open(bytes, sizeof bytes);
  rig.connection().report(open);
  char expected[256];
  std::snprintf(expected, sizeof expected,
                "connection/0 users=2 adapter=%u luid=0\n"
                "connection/0/link/%u users=1 lost=true\n"
                "connection/0/link/%u users=1 lost=false",
                static_cast<unsigned>(rig.connection().adapter()), static_cast<unsigned>(link->device().device()),
                static_cast<unsigned>(fresh->device().device()));
  CHECK(std::strcmp(open.dump(), expected) == 0);
  rig.connection().release(*link);
  rig.connection().release(*fresh); }

TEST(gl_connection, invariants_fail_when_links_devices_and_the_adapter_disagree)
{ { GlContextRig rig;
    DeviceLink *link = nullptr;
    CHECK_STATUS(rig.connection().acquire(link), Status::ok);
    CHECK(link->let_go());
    CHECK(!rig.connection().invariants_hold());
    link->hold();
    CHECK(rig.connection().invariants_hold());
    const std::uint32_t queue = av::GuestConnectionProbe::queue(link->device());
    av::GuestConnectionProbe::set_queue(link->device(), 0u);
    CHECK(!rig.connection().invariants_hold());
    av::GuestConnectionProbe::set_queue(link->device(), queue);
    const std::uint32_t device = av::GuestConnectionProbe::device(link->device());
    av::GuestConnectionProbe::set_device(link->device(), 0u);
    CHECK(!rig.connection().invariants_hold());
    av::GuestConnectionProbe::set_device(link->device(), device);
    CHECK(rig.connection().invariants_hold());
    rig.connection().release(*link); }
  { GlContextRig rig;
    DeviceLink *link = nullptr;
    CHECK_STATUS(rig.connection().acquire(link), Status::ok);
    IcdConnectionProbe::lose_adapter(rig.connection());
    CHECK(!rig.connection().invariants_hold());
    rig.connection().release(*link); }
  { GlContextRig rig;
    CHECK(rig.connection().invariants_hold());
    CHECK_STATUS(IcdConnectionProbe::find_adapter(rig.connection()), Status::ok);
    CHECK(!rig.connection().invariants_hold());
    IcdConnectionProbe::lose_adapter(rig.connection());
    CHECK(rig.connection().invariants_hold()); } }

TEST(gl_connection, invariants_fail_for_every_link_when_a_lost_generation_and_the_current_one_have_no_users)
{ GlContextRig rig;
  DeviceLink *old_link = nullptr;
  CHECK_STATUS(rig.connection().acquire(old_link), Status::ok);
  old_link->lose();
  DeviceLink *fresh = nullptr;
  CHECK_STATUS(rig.connection().acquire(fresh), Status::ok);
  CHECK(old_link != fresh);
  CHECK(rig.connection().invariants_hold());
  CHECK(old_link->let_go());
  CHECK(!rig.connection().invariants_hold());
  CHECK(fresh->let_go());
  CHECK(!rig.connection().invariants_hold());
  old_link->hold();
  fresh->hold();
  CHECK(rig.connection().invariants_hold());
  rig.connection().release(*old_link);
  rig.connection().release(*fresh); }

TEST(gl_connection, a_preferred_luid_picks_the_adapter_before_the_first_acquire_and_is_ignored_after)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  rig.kmt().add_adapter(av::PciIdentity(av::PciIdentity::virtio_vendor, 0x1050u, av::PciIdentity::virtio_vendor,
                                        0x5256u), 3200u);
  rig.kmt().set_adapter_luid(0u, 0x31u);
  rig.kmt().set_adapter_luid(1u, 0x32u);
  connection.prefer(0x32u);
  DeviceLink *link = nullptr;
  CHECK_STATUS(connection.acquire(link), Status::ok);
  std::uint64_t luid = 0u;
  CHECK_STATUS(rig.kmt().adapter_luid(connection.adapter(), luid), Status::ok);
  CHECK_EQ(luid, 0x32u);
  connection.prefer(0x31u);
  connection.release(*link);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }
