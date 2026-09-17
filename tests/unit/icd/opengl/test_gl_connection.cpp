/* The process's one link to the rvkgpu adapter: opened by the first context
 * made, shared by the rest, closed with the last, and found by enumeration. */
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/gl_connection.hpp"

#include <cstdint>
#include <cstring>

using av::GlContextRig;
using av::MockKmt;
using av::Status;
using av::glicd::GlConnection;

namespace av {

struct GuestConnectionProbe {
    static void set_queue(GuestConnection &c, std::uint32_t queue) { c.queue_ = queue; }
    static std::uint32_t queue(const GuestConnection &c) { return c.queue_; }
};

}  // namespace av

namespace av::glicd {

struct GlConnectionProbe {
    static void set_users(GlConnection &connection, std::uint32_t users) { connection.users_ = users; }
    static void lose_adapter(GlConnection &connection) { static_cast<void>(connection.search_.close()); }
};

}  // namespace av::glicd

TEST(gl_connection, invariants_fail_when_users_device_and_adapter_disagree)
{ using av::glicd::GlConnectionProbe;
  { GlContextRig rig;
    CHECK_STATUS(rig.connection().acquire(), Status::ok);
    GlConnectionProbe::set_users(rig.connection(), 0u);
    CHECK(!rig.connection().invariants_hold());
    GlConnectionProbe::set_users(rig.connection(), 1u);
    CHECK(rig.connection().invariants_hold());
    const std::uint32_t queue = av::GuestConnectionProbe::queue(rig.connection().device());
    av::GuestConnectionProbe::set_queue(rig.connection().device(), 0u);
    CHECK(!rig.connection().invariants_hold());
    av::GuestConnectionProbe::set_queue(rig.connection().device(), queue);
    CHECK(rig.connection().invariants_hold());
    rig.connection().release(); }
  { GlContextRig rig;
    CHECK_STATUS(rig.connection().acquire(), Status::ok);
    GlConnectionProbe::lose_adapter(rig.connection());
    CHECK(!rig.connection().invariants_hold());
    rig.connection().release(); } }

TEST(gl_connection, the_first_user_opens_the_device_and_the_last_closes_it)
{ GlContextRig rig;
  GlConnection &connection = rig.connection();
  CHECK(connection.invariants_hold());
  CHECK_EQ(connection.users(), 0u);
  CHECK_STATUS(connection.acquire(), Status::ok);
  CHECK(connection.invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), GlContextRig::link_objects);
  CHECK(connection.device().opened());
  CHECK_STATUS(connection.acquire(), Status::ok);
  CHECK_EQ(connection.users(), 2u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 1u);
  connection.release();
  CHECK(connection.device().opened());
  connection.release();
  CHECK(connection.invariants_hold());
  CHECK(!connection.device().opened());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  connection.release();
  CHECK_EQ(connection.users(), 0u); }

TEST(gl_connection, no_rvkgpu_adapter_or_no_device_is_not_supported_with_nothing_open)
{ GlContextRig rig;
  rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(), Status::not_supported);
  CHECK_EQ(rig.connection().users(), 0u);
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(rig.connection().acquire(), Status::not_supported);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.connection().invariants_hold());
  CHECK_STATUS(rig.connection().acquire(), Status::ok);
  rig.connection().release(); }

TEST(gl_connection, other_adapters_are_not_the_rvkgpu_one)
{ av::MockKmt kmt;
  kmt.add_adapter(av::PciIdentity(0x8086u, 0x46A6u, 0u, 0u), 3200u);
  GlConnection connection(kmt);
  CHECK_STATUS(connection.acquire(), Status::not_supported);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(gl_connection, a_connection_destroyed_with_users_closes_what_it_opened)
{ av::MockKmt kmt;
  kmt.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
  {
    GlConnection connection(kmt);
    CHECK_STATUS(connection.acquire(), Status::ok);
    CHECK_STATUS(connection.acquire(), Status::ok);
  }
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(gl_connection, reports_its_users_and_handles)
{ GlContextRig rig;
  char bytes[128];
  av::StateWriter idle(bytes, sizeof bytes);
  rig.connection().report(idle);
  CHECK(std::strcmp(idle.dump(), "connection/0 users=0 adapter=0 device=0") == 0);
  CHECK_STATUS(rig.connection().acquire(), Status::ok);
  av::StateWriter open(bytes, sizeof bytes);
  rig.connection().report(open);
  CHECK(std::strncmp(open.dump(), "connection/0 users=1 adapter=", 29u) == 0);
  CHECK(std::strstr(open.dump(), " device=0") == nullptr);
  rig.connection().release(); }
