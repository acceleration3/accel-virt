/* The gate between workers raising fence interrupts and the main loop
 * connecting and releasing the notifiers that carry them. */
#include "core/support/status.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/threading/doorbell.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <atomic>
#include <cstdint>
#include <sched.h>

using av::Doorbell;
using av::FenceInterrupts;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace av {

struct FenceInterruptsProbe {
    static void connect_without_lock(FenceInterrupts &interrupts) { interrupts.connected_ = true; }
    static void repair(FenceInterrupts &interrupts) { interrupts.connected_ = false; }
};

}  // namespace av

namespace {

struct Raiser {
    FenceInterrupts           *interrupts;
    std::atomic<bool>          stop{false};
    std::atomic<std::uint32_t> raises{0u};
};

void raise_until_stopped(void *context)
{
    auto *const raiser = static_cast<Raiser *>(context);
    while (!raiser->stop.load()) {
        raiser->interrupts->raise(1u);
        raiser->raises++;
    }
}

}  // namespace

TEST(fence_interrupts_raise, reaches_the_host_only_while_connected)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.connect(), Status::invalid_state);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  CHECK(interrupts.invariants_hold());
  interrupts.raise(2u);
  CHECK_EQ(host.raised(2u), 0u);
  CHECK_STATUS(interrupts.connect(), Status::ok);
  CHECK(interrupts.connected());
  CHECK(interrupts.invariants_hold());
  CHECK_STATUS(interrupts.connect(), Status::ok);   /* already: the host is not asked twice */
  interrupts.raise(2u);
  interrupts.raise(Doorbell::pages - 1u);
  interrupts.raise(Doorbell::pages);                 /* no such line */
  CHECK_EQ(host.raised(2u), 1u);
  CHECK_EQ(host.raised(Doorbell::pages - 1u), 1u);
  interrupts.disconnect();
  CHECK(!interrupts.connected());
  CHECK(interrupts.invariants_hold());
  interrupts.disconnect();
  interrupts.raise(2u);
  CHECK_EQ(host.raised(2u), 1u); }

TEST(fence_interrupts_initialize, a_host_without_a_mutex_or_a_second_call_is_refused)
{ MockHost host;
  FenceInterrupts interrupts(host);
  host.fail_mutex_at(1u);
  CHECK_STATUS(interrupts.initialize(), Status::no_memory);
  CHECK_STATUS(interrupts.connect(), Status::invalid_state);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  CHECK_STATUS(interrupts.initialize(), Status::invalid_state);
  CHECK_EQ(host.live_mutexes(), 1u); }

TEST(fence_interrupts_connect, a_host_that_cannot_connect_leaves_them_off)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  host.fail_fence_connect_at(1u);
  CHECK_STATUS(interrupts.connect(), Status::no_memory);
  CHECK(!interrupts.connected());
  interrupts.raise(0u);
  CHECK_EQ(host.raised(0u), 0u);
  CHECK_STATUS(interrupts.connect(), Status::ok); }

TEST(fence_interrupts, disconnects_and_releases_its_lock_when_destroyed)
{ MockHost host;
  {
      FenceInterrupts interrupts(host);
      CHECK_STATUS(interrupts.initialize(), Status::ok);
      CHECK_STATUS(interrupts.connect(), Status::ok);
  }
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(host.live_mutexes(), 0u);
  FenceInterrupts never_initialized(host); }

TEST(fence_interrupts_raise, workers_raising_across_connects_and_disconnects_never_reach_a_released_notifier)
{ MockHost host;
  FenceInterrupts interrupts(host);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  Raiser raiser;
  raiser.interrupts = &interrupts;
  void *workers[3] = {};
  for (void *&worker : workers) CHECK_STATUS(host.start_thread(&raise_until_stopped, &raiser, worker), Status::ok);
  for (std::uint32_t round = 0u; round < 200u; round++) {
      CHECK_STATUS(interrupts.connect(), Status::ok);
      const std::uint32_t before = raiser.raises.load();
      while (raiser.raises.load() < before + 3u) sched_yield();
      interrupts.disconnect();
  }
  raiser.stop = true;
  for (void *worker : workers) host.join_thread(worker);
  CHECK(raiser.raises.load() > 0u);
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(fence_interrupts, connects_on_the_main_loop)
{ MockHost host;
  FenceInterrupts interrupts(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const i = static_cast<FenceInterrupts *>(context);
                       static_cast<void>(i->initialize());
                       static_cast<void>(i->connect());
                       i->disconnect();
                   },
                   &interrupts, thread),
               Status::ok);
  host.join_thread(thread);
  /* initialize, connect and disconnect, and the host's own connect and disconnect */
  for (int count = 0; count < 5; count++) CHECK(host.claim(HostViolation::main_loop_required)); }

TEST(fence_interrupts_invariants, a_connection_without_a_lock_is_detected)
{ MockHost host;
  FenceInterrupts interrupts(host);
  av::FenceInterruptsProbe::connect_without_lock(interrupts);
  CHECK(!interrupts.invariants_hold());
  av::FenceInterruptsProbe::repair(interrupts); }
