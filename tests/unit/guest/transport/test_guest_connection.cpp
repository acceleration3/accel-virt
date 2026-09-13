/* The adapter, device and paging queue a process opens once and shares across
 * its transports. Opening builds all three; a failure part way leaves nothing
 * open; closing reverses it. Driven over MockKmt. */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

using av::GuestConnection;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::GuestConnectionProbe;

namespace av {

struct GuestConnectionProbe {
    /* A device open with no adapter or queue: what the class never builds. */
    static void orphan(GuestConnection &c) { c.device_ = 9u; c.adapter_ = 0u; c.queue_ = 0u; }
    static void no_queue(GuestConnection &c) { c.device_ = 9u; c.adapter_ = 9u; c.queue_ = 0u; }
};

}  // namespace av

namespace {

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);

}  // namespace

TEST(guest_connection, is_closed_until_opened)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK(!connection.opened());
  CHECK(connection.invariants_hold()); }

TEST(guest_connection, opens_the_adapter_device_and_queue)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(), Status::ok);
  CHECK(connection.opened());
  CHECK(connection.adapter() != 0u);
  CHECK(connection.device() != 0u);
  CHECK(connection.queue() != 0u);
  CHECK(connection.fence_object() != 0u);
  CHECK(&connection.kmt() == &kmt);
  /* Opening again is refused. */
  CHECK_STATUS(connection.open(), Status::invalid_state);
  CHECK(connection.invariants_hold());
  connection.close();
  CHECK(!connection.opened());
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(guest_connection, with_no_adapter_opens_nothing)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(), Status::not_found);
  CHECK(!connection.opened()); }

/* A device that cannot be created leaves the adapter closed again, not leaked. */
TEST(guest_connection, a_failure_part_way_leaves_nothing_open)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  kmt.fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  GuestConnection connection(kmt);
  CHECK(connection.open() != Status::ok);
  CHECK(!connection.opened());
  CHECK_EQ(kmt.open_objects(), 0u); }

/* A paging queue that cannot be created rolls the device and adapter back. */
TEST(guest_connection, a_queue_failure_rolls_back_the_device)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  kmt.fail_at(MockKmt::Call::create_paging_queue, 1u, MockKmt::invalid_parameter);
  GuestConnection connection(kmt);
  CHECK(connection.open() != Status::ok);
  CHECK(!connection.opened());
  CHECK_EQ(kmt.open_objects(), 0u); }

/* close on a connection that never opened is a no-op. */
TEST(guest_connection, closing_an_unopened_connection_is_harmless)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  connection.close();
  CHECK(!connection.opened());
  CHECK(connection.invariants_hold()); }

TEST(guest_connection, an_enumeration_that_fails_opens_nothing)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  GuestConnection connection(kmt);
  CHECK(connection.open() != Status::ok);
  CHECK(!connection.opened()); }

/* An enumeration that hands back a zero adapter handle is no adapter. */
TEST(guest_connection, a_zero_adapter_handle_is_not_found)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  kmt.zero_next(MockKmt::Call::enumerate_adapters);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(), Status::not_found);
  CHECK(!connection.opened()); }

TEST(guest_connection, the_invariant_catches_a_device_with_no_adapter)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK(connection.invariants_hold());
  GuestConnectionProbe::orphan(connection);
  CHECK(!connection.invariants_hold());
  GuestConnection other(kmt);
  GuestConnectionProbe::no_queue(other);
  CHECK(!other.invariants_hold()); }
