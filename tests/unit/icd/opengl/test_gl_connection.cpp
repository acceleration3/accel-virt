/* The process's links to the rvkgpu adapter (US3): the first context made
 * opens a device, the rest share it, the last closes it; a device lost is
 * retired - the contexts on it keep their link until they go, and the next
 * context opens a new device beside it. The adapter is found by enumeration
 * and closed with the last link. */
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

}  // namespace av

namespace av::icd {

struct IcdConnectionProbe {
    static void set_users(av::icd::IcdConnection &connection, std::uint32_t link, std::uint32_t users)
    {
        connection.users_[link] = users;
    }
    static void lose_adapter(av::icd::IcdConnection &connection) { static_cast<void>(connection.search_.close()); }
    static void set_lost(av::icd::IcdConnection &connection, std::uint32_t link, bool lost) { connection.lost_[link] = lost; }
    static void set_current(av::icd::IcdConnection &connection, std::uint32_t link) { connection.current_ = link; }
};

}  // namespace av::glicd

using av::icd::IcdConnectionProbe;

namespace {

constexpr std::uint32_t no_link = av::icd::IcdConnection::no_link;

}  // namespace

TEST(gl_connection, the_first_user_opens_a_device_the_rest_share_it_and_the_last_closes_it)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  CHECK(connection.invariants_hold());
  CHECK_EQ(connection.users(), 0u);
  std::uint32_t first = no_link;
  CHECK_STATUS(connection.acquire(first), Status::ok);
  CHECK(first < av::icd::IcdConnection::max_links);
  CHECK(connection.invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), GlContextRig::link_objects);
  CHECK(connection.device(first).opened());
  std::uint32_t second = no_link;
  CHECK_STATUS(connection.acquire(second), Status::ok);
  CHECK_EQ(second, first);
  CHECK_EQ(connection.users(), 2u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 1u);
  CHECK(!connection.lost(first));
  connection.release(first);
  CHECK(connection.device(first).opened());
  CHECK(connection.invariants_hold());
  connection.release(second);
  CHECK(connection.invariants_hold());
  CHECK_EQ(connection.users(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::close_adapter), 1u);
  /* Releasing what is not held, or no link at all, changes nothing. */
  connection.release(first);
  connection.release(no_link);
  connection.release(0xFFFFFFFFu);
  CHECK_EQ(connection.users(), 0u);
  CHECK(connection.invariants_hold()); }

/* A lost device takes no new users: the next context opens a device of its
 * own on the same adapter while the lost one's contexts still hold theirs,
 * and each device closes with its own last user. */
TEST(gl_connection, a_lost_device_is_retired_and_the_next_user_opens_a_new_one)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  std::uint32_t old_link = no_link;
  CHECK_STATUS(connection.acquire(old_link), Status::ok);
  std::uint32_t sharer = no_link;
  CHECK_STATUS(connection.acquire(sharer), Status::ok);
  connection.lose(old_link);
  CHECK(connection.invariants_hold());
  CHECK(connection.lost(old_link));
  CHECK(connection.lost(sharer));
  CHECK(connection.device(old_link).opened());
  std::uint32_t fresh = no_link;
  CHECK_STATUS(connection.acquire(fresh), Status::ok);
  CHECK(fresh != old_link);
  CHECK(!connection.lost(fresh));
  CHECK(connection.invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 2u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 1u);
  CHECK_EQ(connection.users(), 3u);
  /* Losing it twice, or losing a link nobody holds, changes nothing. */
  connection.lose(old_link);
  connection.lose(no_link);
  std::uint32_t idle = 0u;
  while (idle == old_link || idle == fresh) idle++;
  connection.lose(idle);
  CHECK(!connection.lost(idle));
  CHECK(!connection.lost(no_link));
  CHECK(connection.invariants_hold());
  connection.release(old_link);
  connection.release(sharer);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_device), 1u);
  CHECK(!connection.lost(old_link));
  CHECK(connection.device(fresh).opened());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::close_adapter), 0u);
  connection.release(fresh);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(connection.invariants_hold()); }

/* With every link lost and still held, a new context has nowhere to go. */
TEST(gl_connection, every_link_lost_and_held_leaves_no_room)
{ GlContextRig rig;
  av::icd::IcdConnection &connection = rig.connection();
  std::uint32_t links[av::icd::IcdConnection::max_links] = {};
  for (std::uint32_t &link : links) {
    CHECK_STATUS(connection.acquire(link), Status::ok);
    connection.lose(link);
    CHECK(connection.invariants_hold());
  }
  std::uint32_t refused = 7u;
  CHECK_STATUS(connection.acquire(refused), Status::no_space);
  CHECK_EQ(refused, no_link);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), av::icd::IcdConnection::max_links);
  connection.release(links[2]);
  std::uint32_t again = no_link;
  CHECK_STATUS(connection.acquire(again), Status::ok);
  CHECK_EQ(again, links[2]);
  for (std::uint32_t &link : links) connection.release(link);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(connection.invariants_hold()); }

TEST(gl_connection, no_rvkgpu_adapter_or_no_device_is_not_supported_with_nothing_open)
{ GlContextRig rig;
  std::uint32_t link = 3u;
  rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(link), Status::not_supported);
  CHECK_EQ(link, no_link);
  CHECK_EQ(rig.connection().users(), 0u);
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(link), Status::not_supported);
  CHECK_EQ(link, no_link);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.connection().invariants_hold());
  /* A second device that cannot be made leaves the first, and the adapter, open. */
  std::uint32_t first = no_link;
  CHECK_STATUS(rig.connection().acquire(first), Status::ok);
  rig.connection().lose(first);
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(link), Status::not_supported);
  CHECK(rig.connection().device(first).opened());
  CHECK_EQ(rig.kmt().open_objects(), GlContextRig::link_objects);
  CHECK(rig.connection().invariants_hold());
  rig.connection().release(first);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_connection, other_adapters_are_not_the_rvkgpu_one)
{ av::MockKmt kmt;
  kmt.add_adapter(av::PciIdentity(0x8086u, 0x46A6u, 0u, 0u), 3200u);
  av::icd::IcdConnection connection(kmt);
  std::uint32_t link = 0u;
  CHECK_STATUS(connection.acquire(link), Status::not_supported);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(gl_connection, a_connection_destroyed_with_users_closes_what_it_opened)
{ av::MockKmt kmt;
  kmt.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
  {
    av::icd::IcdConnection connection(kmt);
    std::uint32_t link = no_link;
    CHECK_STATUS(connection.acquire(link), Status::ok);
    CHECK_STATUS(connection.acquire(link), Status::ok);
    connection.lose(link);
    CHECK_STATUS(connection.acquire(link), Status::ok);
  }
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(gl_connection, reports_its_users_and_each_link)
{ GlContextRig rig;
  char bytes[256];
  av::StateWriter idle(bytes, sizeof bytes);
  rig.connection().report(idle);
  CHECK(std::strcmp(idle.dump(), "connection/0 users=0 adapter=0") == 0);
  std::uint32_t link = no_link;
  CHECK_STATUS(rig.connection().acquire(link), Status::ok);
  rig.connection().lose(link);
  std::uint32_t fresh = no_link;
  CHECK_STATUS(rig.connection().acquire(fresh), Status::ok);
  av::StateWriter open(bytes, sizeof bytes);
  rig.connection().report(open);
  char expected[256];
  std::snprintf(expected, sizeof expected,
                "connection/0 users=2 adapter=%u\n"
                "connection/0/link/%u users=1 lost=true device=%u\n"
                "connection/0/link/%u users=1 lost=false device=%u",
                static_cast<unsigned>(rig.connection().device(link).adapter()), static_cast<unsigned>(link),
                static_cast<unsigned>(rig.connection().device(link).device()), static_cast<unsigned>(fresh),
                static_cast<unsigned>(rig.connection().device(fresh).device()));
  CHECK(std::strcmp(open.dump(), expected) == 0);
  rig.connection().release(link);
  rig.connection().release(fresh); }

TEST(gl_connection, invariants_fail_when_links_devices_and_the_adapter_disagree)
{ { GlContextRig rig;
    std::uint32_t link = no_link;
    CHECK_STATUS(rig.connection().acquire(link), Status::ok);
    IcdConnectionProbe::set_users(rig.connection(), link, 0u);
    CHECK(!rig.connection().invariants_hold());
    IcdConnectionProbe::set_users(rig.connection(), link, 1u);
    CHECK(rig.connection().invariants_hold());
    const std::uint32_t queue = av::GuestConnectionProbe::queue(rig.connection().device(link));
    av::GuestConnectionProbe::set_queue(rig.connection().device(link), 0u);
    CHECK(!rig.connection().invariants_hold());
    av::GuestConnectionProbe::set_queue(rig.connection().device(link), queue);
    /* A held link whose device is not open. */
    const std::uint32_t device = av::GuestConnectionProbe::device(rig.connection().device(link));
    av::GuestConnectionProbe::set_device(rig.connection().device(link), 0u);
    CHECK(!rig.connection().invariants_hold());
    av::GuestConnectionProbe::set_device(rig.connection().device(link), device);
    CHECK(rig.connection().invariants_hold());
    /* A held link that is not lost is the one new users join. */
    IcdConnectionProbe::set_current(rig.connection(), no_link);
    CHECK(!rig.connection().invariants_hold());
    IcdConnectionProbe::set_current(rig.connection(), link);
    CHECK(rig.connection().invariants_hold());
    rig.connection().release(link); }
  { GlContextRig rig;
    std::uint32_t link = no_link;
    CHECK_STATUS(rig.connection().acquire(link), Status::ok);
    IcdConnectionProbe::lose_adapter(rig.connection());
    CHECK(!rig.connection().invariants_hold());
    rig.connection().release(link); }
  { GlContextRig rig;
    /* A lost link nobody holds, and a current link that is lost. */
    IcdConnectionProbe::set_lost(rig.connection(), 1u, true);
    CHECK(!rig.connection().invariants_hold());
    IcdConnectionProbe::set_lost(rig.connection(), 1u, false);
    std::uint32_t link = no_link;
    CHECK_STATUS(rig.connection().acquire(link), Status::ok);
    IcdConnectionProbe::set_lost(rig.connection(), link, true);
    CHECK(!rig.connection().invariants_hold());
    IcdConnectionProbe::set_lost(rig.connection(), link, false);
    CHECK(rig.connection().invariants_hold());
    /* A current link nobody holds. */
    rig.connection().release(link);
    IcdConnectionProbe::set_current(rig.connection(), link);
    CHECK(!rig.connection().invariants_hold());
    IcdConnectionProbe::set_current(rig.connection(), no_link);
    CHECK(rig.connection().invariants_hold()); } }
