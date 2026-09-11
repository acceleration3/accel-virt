/* The byte the guest writes to the virtio status register, and the fence
 * interrupts that follow it: connected while DRIVER_OK stands, released the
 * moment it is cleared. */
#include "core/support/status.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/module/driver_status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>
#include <cstring>

using av::DriverStatus;
using av::FenceInterrupts;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace {

/* The sequence a driver writes on its way up: acknowledge, driver, features
 * ok, then DRIVER_OK. Nothing before the last one connects anything. */
constexpr std::uint8_t acknowledge = 0x01u;
constexpr std::uint8_t driver = 0x03u;
constexpr std::uint8_t features_ok = 0x0Bu;
constexpr std::uint8_t running = 0x0Fu;

}  // namespace

TEST(driver_status_set, connects_only_once_driver_ok_is_written)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  DriverStatus status(host, interrupts);
  CHECK_EQ(status.value(), 0u);
  CHECK(status.invariants_hold());
  constexpr std::uint8_t bringing_up[] = {acknowledge, driver, features_ok};
  for (const std::uint8_t byte : bringing_up) {
      status.set(byte);
      CHECK_EQ(status.value(), byte);
      CHECK(!host.fence_interrupts_connected());
      CHECK(status.invariants_hold());
  }
  status.set(running);
  CHECK(host.fence_interrupts_connected());
  CHECK_EQ(status.value(), running);
  CHECK(status.invariants_hold());
  status.reset();
  CHECK(!host.fence_interrupts_connected()); }

/* 035's live run: viogpudo stopped with status 11 and no reset, so nothing was
 * released and the next driver's MSI-X toggle registered the same eventfd
 * twice. The clear is what must release them. */
TEST(driver_status_set, clearing_driver_ok_releases_the_fence_interrupts)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  DriverStatus status(host, interrupts);
  status.set(running);
  CHECK(host.fence_interrupts_connected());
  interrupts.raise(1u);
  CHECK_EQ(host.raised(1u), 1u);
  status.set(features_ok);
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(status.value(), features_ok);
  CHECK(status.invariants_hold());
  interrupts.raise(1u);
  CHECK_EQ(host.raised(1u), 1u);   /* dropped, not injected */
  status.set(running);
  CHECK(host.fence_interrupts_connected());
  CHECK(status.invariants_hold());
  interrupts.raise(1u);
  CHECK_EQ(host.raised(1u), 2u);
  status.reset(); }

/* MockHost records a fence_misuse for a second connect or a release of what is
 * not connected, so a repeat that reached the host would fail this test. */
TEST(driver_status_set, a_repeated_status_asks_the_host_nothing)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  DriverStatus status(host, interrupts);
  status.set(features_ok);
  status.set(features_ok);
  CHECK(!host.fence_interrupts_connected());
  status.set(running);
  status.set(running);
  status.set(running);
  CHECK(host.fence_interrupts_connected());
  CHECK(status.invariants_hold());
  status.reset();
  status.reset();
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(status.value(), 0u);
  CHECK_EQ(host.unclaimed_violations(), 0u); }

/* Status 0 is the reset the guest writes before QEMU calls reset: the pair
 * releases the interrupts exactly once. */
TEST(driver_status_set, status_zero_releases_them_and_the_reset_that_follows_is_idempotent)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  DriverStatus status(host, interrupts);
  status.set(running);
  status.set(0u);
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(status.value(), 0u);
  CHECK(status.invariants_hold());
  status.reset();
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(driver_status_set, a_host_that_cannot_route_logs_the_refusal_with_the_status_that_caused_it)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  DriverStatus status(host, interrupts);
  host.fail_fence_connect_at(1u);
  status.set(running);
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(host.logged(), 1u);
  CHECK(std::strstr(host.last_log(), "15") != nullptr);
  CHECK_EQ(status.value(), running);
  CHECK(status.invariants_hold());
  status.set(running);
  CHECK(host.fence_interrupts_connected());
  CHECK_EQ(host.logged(), 1u);
  status.reset(); }

TEST(driver_status, is_written_on_the_main_loop_only)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  DriverStatus status(host, interrupts);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const written = static_cast<DriverStatus *>(context);
                       written->set(running);
                       written->reset();
                   },
                   &status, thread),
               Status::ok);
  host.join_thread(thread);
  /* set and reset, the FenceInterrupts call under each, and the host's own */
  for (int count = 0; count < 6; count++) CHECK(host.claim(HostViolation::main_loop_required)); }

TEST(driver_status_invariants, a_connection_the_status_does_not_account_for_is_detected)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  DriverStatus status(host, interrupts);
  CHECK_STATUS(interrupts.connect(), Status::ok);   /* connected behind its back */
  CHECK(!status.invariants_hold());
  interrupts.disconnect();
  CHECK(status.invariants_hold()); }
