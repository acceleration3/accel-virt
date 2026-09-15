/* The in-guest suite on Linux: GuestSuite, the program rvktest.exe runs in the
 * Windows guest, run unchanged over DdiKmt against the real driver and the
 * real device core. Each run is judged by the report the guest runner reads,
 * line by line where a line carries a measurement. */
#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_suite.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::FullStackRig;
using av::GuestReport;
using av::GuestSuite;

namespace {

/* Whether the report holds `line` as a whole line. */
bool reported(const GuestReport &report, const char *line)
{
    const std::size_t length = std::strlen(line);
    for (const char *at = std::strstr(report.text(), line); at != nullptr; at = std::strstr(at + 1, line)) {
        const bool starts = at == report.text() || at[-1] == '\n';
        if (starts && (at[length] == '\n' || at[length] == '\0')) return true;
    }
    return false;
}

void print(const GuestReport &report)
{
    std::fwrite(report.text(), 1u, report.length(), stderr);
}

}  // namespace

/* Every scenario rvktest.exe runs in the guest passes here too, with the same
 * 41 steps the 043 guest run reported. host_visible passes as it does in the
 * guest today: its allocation is a client allocation VidMm keeps in system
 * memory (feature 041 parked the CPU-visible segment), so the lock is a view
 * of guest pages, not of the hostmem BAR. vsync measures the driver's timer
 * period on the mock clock, which only the wait moves: it proves the period
 * the driver asks for, not how a host timer keeps it. */
TEST(fullstack_suite, the_in_guest_suite_passes_over_the_real_driver)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  GuestReport report;
  std::uint32_t failed = 0u;
  { GuestSuite suite(rig.kmt(), report, 0u);
    failed = suite.run(); }
  if (failed != 0u) print(report);
  CHECK_EQ(failed, 0u);
  CHECK_EQ(report.passed(), 43u);
  CHECK(reported(report, "rvktest: 43 passed, 0 failed"));
  CHECK(reported(report, "ok adapter.driver_version"));
  CHECK(reported(report, "ok allocation.refuse_hostile"));
  CHECK(reported(report, "ok mapping.map"));
  CHECK(reported(report, "ok submission.wait"));
  CHECK(reported(report, "ok host_visible.pattern 4096"));
  CHECK(std::strstr(report.text(), "\nok vsync.rate 60") != nullptr);
  CHECK(reported(report, "ok vsync.caught 6000"));
  CHECK(reported(report, "ok transport.nop 0"));
  CHECK(std::strstr(report.text(), "\nok transport.latency ") != nullptr);
  /* The device core answered the no-op itself: the executor behind it ran nothing. */
  CHECK_EQ(rig.device().executor().executed(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* The long fence run, for one second of the mock clock: a submission and wait
 * at a time, a vertical blank after every 64, so the clock moves only at the
 * blanks and the run is exactly sixty of them. The fences count and the blanks
 * are the driver's; worst_wait_us is not a measurement here, because nothing
 * moves the clock while a fence is awaited. */
TEST(fullstack_suite, the_fence_stress_run_completes_every_fence_it_submits)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  GuestReport report;
  std::uint32_t failed = 0u;
  { GuestSuite suite(rig.kmt(), report, 1u);
    failed = suite.run(); }
  if (failed != 0u) print(report);
  CHECK_EQ(failed, 0u);
  CHECK(reported(report, "ok stress.fences 3840"));
  CHECK(reported(report, "ok stress.blanks 60")); }

/* The timeout run cannot pass on this rig, and this says exactly where. The
 * scenario needs dxgkrnl's timeout detection - VidSch seeing the held node
 * make no progress past TdrDelay, QueryEngineStatus, ResetEngine and the held
 * wait failed - and a host told to hold one context. The rig has neither: its
 * sessions' contexts carry no rvk record, so the driver completes them in its
 * DPC, and nothing ever times a node out. The held wait completes instead of
 * being refused; every other step passes. */
TEST(fullstack_suite, the_timeout_run_fails_only_where_the_rig_has_no_tdr)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  GuestReport report;
  std::uint32_t failed = 0u;
  { GuestSuite suite(rig.kmt(), report, 0u);
    failed = suite.run_timeout(); }
  CHECK_EQ(failed, 1u);
  CHECK_EQ(report.passed(), 11u);
  CHECK(reported(report, "FAIL timeout.held_lost status=0 code=0x00000000"));
  CHECK(reported(report, "ok timeout.other_completed")); }
