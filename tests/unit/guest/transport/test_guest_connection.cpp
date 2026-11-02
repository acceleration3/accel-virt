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
    static void orphan(GuestConnection &c) { c.device_ = 9u; c.adapter_ = 0u; c.queue_ = 0u; }
    static void no_queue(GuestConnection &c) { c.device_ = 9u; c.adapter_ = 9u; c.queue_ = 0u; }
};

}

namespace {

const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint32_t no_memory = 0xC0000017u;

}

TEST(guest_connection, is_closed_until_opened)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK(!connection.opened());
  CHECK(connection.invariants_hold()); }

namespace {
std::uint32_t found_adapter(MockKmt &kmt)
{
    std::uint32_t adapters[1] = {};
    std::uint32_t count = 0u;
    CHECK_STATUS(kmt.enumerate_adapters(adapters, 1u, count), Status::ok);
    CHECK_EQ(count, 1u);
    return adapters[0];
}

}

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

TEST(guest_connection, adapter_zero_is_refused)
{ MockKmt kmt;
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(0u), Status::invalid_parameter);
  CHECK(connection.invariants_hold());
  CHECK(!connection.opened());
  CHECK_EQ(connection.adapter(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }

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
