#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_suite.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using av::FullStackRig;
using av::GuestReport;
using av::GuestSuite;

namespace {
bool reported(const GuestReport &report, const char *line)
{
    const std::size_t length = std::strlen(line);
    for (const char *at = std::strstr(report.text(), line); at != nullptr; at = std::strstr(at + 1, line)) {
        const bool starts = at == report.text() || at[-1] == '\n';
        if (starts && (at[length] == '\n' || at[length] == '\0')) return true;
    }
    return false;
}

std::uint64_t measured(const GuestReport &report, const char *name)
{
    char prefix[64];
    std::snprintf(prefix, sizeof prefix, "\nok %s ", name);
    const char *const at = std::strstr(report.text(), prefix);
    return at == nullptr ? 0u : std::strtoull(at + std::strlen(prefix), nullptr, 10);
}

void print(const GuestReport &report)
{
    std::fwrite(report.text(), 1u, report.length(), stderr);
}

}

TEST(fullstack_suite, the_in_guest_suite_passes_over_the_real_driver)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  std::uint32_t failed = 0u;
  { GuestSuite suite(rig.kmt(), report, 0u);
    failed = suite.run(); }
  if (failed != 0u) print(report);
  CHECK_EQ(failed, 0u);
  CHECK_EQ(report.passed(), 37u);
  CHECK(reported(report, "avtest: 37 passed, 0 failed"));
  CHECK(reported(report, "ok adapter.driver_version"));
  CHECK(reported(report, "ok allocation.refuse_hostile"));
  CHECK(reported(report, "ok mapping.map"));
  CHECK(reported(report, "ok submission.wait"));
  CHECK(std::strstr(report.text(), "\nok vsync.rate 60") != nullptr);
  CHECK(measured(report, "vsync.caught") >= 5995u && measured(report, "vsync.caught") <= 6000u);
  CHECK(reported(report, "ok transport.nop 0"));
  CHECK(std::strstr(report.text(), "\nok transport.latency ") != nullptr);
  CHECK(std::strstr(report.text(), "\nok transport.warm_nop ") != nullptr);
  CHECK_EQ(rig.device().executor().executed(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_suite, the_fence_stress_run_completes_every_fence_it_submits)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  std::uint32_t failed = 0u;
  { GuestSuite suite(rig.kmt(), report, 1u);
    failed = suite.run(); }
  if (failed != 0u) print(report);
  CHECK_EQ(failed, 0u);
  const std::uint64_t blanks = measured(report, "stress.blanks");
  const std::uint64_t fences = measured(report, "stress.fences");
  CHECK(blanks >= 40u && blanks <= 61u);
  CHECK(fences >= blanks * 64u && fences < (blanks + 1u) * 64u); }

TEST(fullstack_suite, the_timeout_run_fails_only_where_the_rig_has_no_tdr)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  std::uint32_t failed = 0u;
  { GuestSuite suite(rig.kmt(), report, 0u);
    failed = suite.run_timeout(); }
  CHECK_EQ(failed, 1u);
  CHECK_EQ(report.passed(), 11u);
  CHECK(reported(report, "FAIL timeout.held_lost status=0 code=0x00000000"));
  CHECK(reported(report, "ok timeout.other_completed")); }
