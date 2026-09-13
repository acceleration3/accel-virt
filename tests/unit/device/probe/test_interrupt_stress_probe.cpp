/* US3: fence interrupts under the guest's own masking. The device counts what
 * it raised and what the guest said it saw; the probe judges the pair. */
#include "core/support/status.hpp"
#include "device/probe/interrupt_stress_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::HostViolation;
using av::InterruptStressProbe;
using av::MockHost;
using av::Status;

TEST(interrupt_stress_probe_verdict, a_run_whose_last_fence_was_acknowledged_passes)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  for (std::uint32_t fence = 0u; fence < 1000u; fence++) probe.raised(fence % 4u);
  probe.acknowledged(1000u, 250000u);
  probe.finish();
  CHECK(probe.verdict().passed());
  CHECK_EQ(probe.verdict().measured(), 250000u); }

/* An interrupt the guest was not listening for is the defect this run exists to
 * find: the guest disabled its interrupt and the device injected anyway. */
TEST(interrupt_stress_probe_verdict, an_interrupt_raised_while_the_guest_was_disabled_fails)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  probe.raised(0u);
  probe.disabled();
  probe.raised(1u);
  probe.enabled();
  probe.acknowledged(2u, 100u);
  probe.finish();
  CHECK(!probe.verdict().passed());
  CHECK_STATUS(probe.verdict().reason(), Status::invalid_state);
  CHECK_EQ(probe.verdict().errno_value(), 1); }

TEST(interrupt_stress_probe_verdict, nothing_raised_while_disabled_is_not_a_failure)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  probe.raised(0u);
  probe.disabled();
  probe.enabled();
  probe.raised(1u);
  probe.acknowledged(2u, 100u);
  probe.finish();
  CHECK(probe.verdict().passed()); }

/* A lost final interrupt is the classic one: the guest waits forever because
 * the last fence's wakeup never arrived. */
TEST(interrupt_stress_probe_verdict, a_run_the_guest_did_not_see_the_end_of_fails)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  for (std::uint32_t fence = 0u; fence < 10u; fence++) probe.raised(0u);
  probe.acknowledged(9u, 100u);
  probe.finish();
  CHECK(!probe.verdict().passed());
  CHECK_STATUS(probe.verdict().reason(), Status::not_found);
  CHECK_EQ(probe.verdict().errno_value(), 1); }

TEST(interrupt_stress_probe_verdict, a_guest_that_saw_more_than_was_raised_fails)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  probe.raised(0u);
  probe.acknowledged(2u, 100u);
  probe.finish();
  CHECK(!probe.verdict().passed());
  CHECK_STATUS(probe.verdict().reason(), Status::malformed); }

/* A run nobody finished has nothing to say, and finishing twice does not
 * change what it said. */
TEST(interrupt_stress_probe_verdict, a_run_that_was_never_finished_never_ran)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  probe.raised(0u);
  CHECK(!probe.verdict().ran());
  probe.acknowledged(1u, 10u);
  probe.finish();
  CHECK(probe.verdict().passed());
  probe.raised(0u);
  probe.finish();
  CHECK(probe.verdict().passed()); }

TEST(interrupt_stress_probe_acknowledged, keeps_the_worst_delay_of_the_run)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  probe.raised(0u);
  probe.raised(0u);
  probe.raised(0u);
  probe.acknowledged(1u, 400u);
  probe.acknowledged(2u, 9000u);
  probe.acknowledged(3u, 700u);
  probe.finish();
  CHECK(probe.verdict().passed());
  CHECK_EQ(probe.verdict().measured(), 9000u); }

/* Workers raise; the main loop counts what the guest said. */
TEST(interrupt_stress_probe, raises_from_workers_and_judges_on_the_main_loop)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  void *first = nullptr;
  void *second = nullptr;
  const auto raise = [](void *context) {
      for (std::uint32_t fence = 0u; fence < 500u; fence++) static_cast<InterruptStressProbe *>(context)->raised(1u);
  };
  CHECK_STATUS(host.start_thread(raise, &probe, first), Status::ok);
  CHECK_STATUS(host.start_thread(raise, &probe, second), Status::ok);
  host.join_thread(first);
  host.join_thread(second);
  probe.acknowledged(1000u, 5u);
  probe.finish();
  CHECK(probe.verdict().passed());
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(interrupt_stress_probe, the_guests_own_reports_are_main_loop_only)
{ MockHost host;
  InterruptStressProbe probe(host);
  CHECK_STATUS(probe.initialize(), Status::ok);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const stress = static_cast<InterruptStressProbe *>(context);
                       stress->disabled();
                       stress->enabled();
                       stress->acknowledged(0u, 0u);
                       stress->finish();
                   },
                   &probe, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  while (host.claim(HostViolation::main_loop_required)) {} }

/* A probe with no lock has nothing to count with: every call is a no-op rather
 * than a crash, because a host with no mutex still has to run. */
TEST(interrupt_stress_probe, without_a_lock_it_counts_nothing_and_judges_nothing)
{ MockHost host;
  InterruptStressProbe probe(host);
  probe.raised(0u);
  probe.disabled();
  probe.enabled();
  probe.acknowledged(5u, 5u);
  probe.finish();
  CHECK(!probe.verdict().ran());
  host.fail_mutex_at(1u);
  InterruptStressProbe refused(host);
  CHECK_STATUS(refused.initialize(), Status::no_memory);
  CHECK_STATUS(refused.initialize(), Status::ok);
  CHECK_STATUS(refused.initialize(), Status::invalid_state); }
