/* The device and paging queue a process opens once, on the adapter it found,
 * and shares across its transports. Opening builds both; a failure part way
 * leaves nothing open; closing reverses it and leaves the adapter to its
 * owner. Driven over MockKmt. */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>

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

const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
/* STATUS_NO_MEMORY: the driver's own error, told apart from the connection's
 * refusal of adapter 0. */
constexpr std::uint32_t no_memory = 0xC0000017u;

}  // namespace

TEST(guest_connection, is_closed_until_opened)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK(!connection.opened());
  CHECK(connection.invariants_hold()); }

namespace {

/* The adapter a caller found: the connection is handed it, never enumerates. */
std::uint32_t found_adapter(MockKmt &kmt)
{
    std::uint32_t adapters[1] = {};
    std::uint32_t count = 0u;
    CHECK_STATUS(kmt.enumerate_adapters(adapters, 1u, count), Status::ok);
    CHECK_EQ(count, 1u);
    return adapters[0];
}

}  // namespace

TEST(guest_connection, opens_a_device_and_queue_on_the_adapter_it_is_handed)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  const std::uint32_t adapter = found_adapter(kmt);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::ok);
  CHECK(connection.invariants_hold());
  CHECK(connection.opened());
  CHECK_EQ(connection.adapter(), adapter);
  CHECK(connection.device() != 0u);
  CHECK(connection.queue() != 0u);
  CHECK(connection.fence_object() != 0u);
  CHECK(&connection.kmt() == &kmt);
  CHECK_EQ(kmt.calls(MockKmt::Call::enumerate_adapters), 1u);
  /* The adapter, the device and the queue. */
  CHECK_EQ(kmt.open_objects(), 3u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(guest_connection, opening_again_is_refused_and_keeps_the_first_device)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  kmt.add_adapter(av_gpu, 3200u);
  std::uint32_t adapters[2] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 2u, count), Status::ok);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapters[0]), Status::ok);
  const std::uint32_t device = connection.device();
  CHECK_STATUS(connection.open(adapters[1]), Status::invalid_state);
  CHECK(connection.invariants_hold());
  CHECK_EQ(connection.adapter(), adapters[0]);
  CHECK_EQ(connection.device(), device);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 1u); }

/* Adapter 0 is no adapter: refused before the driver is asked for anything. */
TEST(guest_connection, adapter_zero_is_refused)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(0u), Status::invalid_parameter);
  CHECK(connection.invariants_hold());
  CHECK(!connection.opened());
  CHECK_EQ(connection.adapter(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }

/* A device that cannot be created leaves nothing open but the caller's adapter. */
TEST(guest_connection, a_device_failure_leaves_only_the_adapter_open)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  const std::uint32_t adapter = found_adapter(kmt);
  kmt.fail_at(MockKmt::Call::create_device, 1u, no_memory);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::no_memory);
  CHECK(connection.invariants_hold());
  CHECK(!connection.opened());
  CHECK_EQ(connection.adapter(), 0u);
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_paging_queue), 0u); }

/* A paging queue that cannot be created rolls the device back; the adapter stays. */
TEST(guest_connection, a_queue_failure_rolls_back_the_device)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  const std::uint32_t adapter = found_adapter(kmt);
  kmt.fail_at(MockKmt::Call::create_paging_queue, 1u, no_memory);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::no_memory);
  CHECK(connection.invariants_hold());
  CHECK(!connection.opened());
  CHECK_EQ(kmt.calls(MockKmt::Call::destroy_device), 1u);
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_EQ(kmt.misuses(), 0u); }

/* The adapter is the caller's: closing destroys the queue and device only, and
 * the caller closes the adapter itself. */
TEST(guest_connection, close_leaves_the_adapter_open)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3200u);
  const std::uint32_t adapter = found_adapter(kmt);
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::ok);
  connection.close();
  CHECK(connection.invariants_hold());
  CHECK(!connection.opened());
  CHECK_EQ(connection.adapter(), 0u);
  CHECK_EQ(connection.device(), 0u);
  CHECK_EQ(connection.queue(), 0u);
  CHECK_EQ(connection.fence_object(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::destroy_paging_queue), 1u);
  CHECK_EQ(kmt.calls(MockKmt::Call::destroy_device), 1u);
  CHECK_EQ(kmt.calls(MockKmt::Call::close_adapter), 0u);
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(kmt.close_adapter(adapter), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

/* close on a connection that never opened is a no-op. */
TEST(guest_connection, closing_an_unopened_connection_is_harmless)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  connection.close();
  CHECK(!connection.opened());
  CHECK(connection.invariants_hold());
  CHECK_EQ(kmt.calls(MockKmt::Call::destroy_paging_queue), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::destroy_device), 0u); }

TEST(guest_connection, the_invariant_catches_a_device_with_no_adapter)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK(connection.invariants_hold());
  GuestConnectionProbe::orphan(connection);
  CHECK(!connection.invariants_hold());
  GuestConnection other(kmt);
  GuestConnectionProbe::no_queue(other);
  CHECK(!other.invariants_hold()); }
