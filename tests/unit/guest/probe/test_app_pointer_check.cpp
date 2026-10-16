/* The judgement every API's application-memory step shares: a pattern the
 * application wrote, what it read back after the GPU's copy, and whether the
 * API's mapping was the application's own pointer. */
#include "guest/probe/app_pointer_check.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace av {

struct AppPointerCheckProbe {
    static std::uint8_t *pattern(AppPointerCheck &check) { return check.pattern_; }
};

}  // namespace av

using av::AppPointerCheck;
using av::AppPointerCheckProbe;
using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;

namespace {

const char *const names[3] = {"heap", "alias", "copy"};

class CheckRig {
public:
    CheckRig() : report_("avprobe"), steps_(kmt_, report_, "api") {}
    CheckRig(const CheckRig &) = delete;
    CheckRig &operator=(const CheckRig &) = delete;

    AppPointerCheck &check() { return check_; }
    const char *judge(std::int32_t result, std::uint32_t aliased)
    {
        check_.judge(steps_, names, result, aliased);
        report_.finish();
        return report_.text();
    }
    /* What a working driver reads back: the pattern itself. */
    void copy_through()
    { std::memcpy(check_.copied(), check_.pattern(), AppPointerCheck::bytes); }

private:
    MockKmt         kmt_;
    GuestReport     report_;
    ScenarioSteps   steps_;
    AppPointerCheck check_;
};

}  // namespace

/* A pattern no page repeats: a page copied to the wrong place is caught. */
TEST(app_pointer_check, the_pattern_differs_from_page_to_page)
{ AppPointerCheck check;
  CHECK(check.invariants_hold());
  const std::uint8_t *const pattern = check.pattern();
  std::uint32_t same = 0u;
  for (std::uint32_t byte = 0u; byte < 4096u; byte++) same += pattern[byte] == pattern[4096u + byte] ? 1u : 0u;
  CHECK(same < 64u);
  CHECK(pattern[0] != pattern[1]);
  /* Nothing read back yet is nothing matching but chance. */
  CHECK(check.matching() < AppPointerCheck::bytes); }

TEST(app_pointer_check, every_byte_back_through_the_applications_pointer_passes)
{ CheckRig rig;
  rig.copy_through();
  const char *report = rig.judge(0, 1u);
  CHECK(std::strstr(report, "ok api.heap") != nullptr);
  CHECK(std::strstr(report, "ok api.alias 1") != nullptr);
  CHECK(std::strstr(report, "ok api.copy 65536") != nullptr);
  CHECK(std::strstr(report, "avprobe: 3 passed, 0 failed") != nullptr); }

/* The first and the last byte each count: a copy one short at either end, or
 * a mapping that is the driver's own, fails with what was measured. */
TEST(app_pointer_check, a_byte_lost_at_either_end_or_a_mapping_elsewhere_fails)
{ for (const std::uint32_t at : {0u, AppPointerCheck::bytes - 1u}) {
      CheckRig rig;
      rig.copy_through();
      rig.check().copied()[at] = static_cast<std::uint8_t>(rig.check().copied()[at] ^ 0xFFu);
      const char *report = rig.judge(0, 1u);
      CHECK(std::strstr(report, "FAIL api.copy") != nullptr);
      CHECK(std::strstr(report, " 65535\n") != nullptr);
  }
  CheckRig elsewhere;
  elsewhere.copy_through();
  const char *report = elsewhere.judge(0, 0u);
  CHECK(std::strstr(report, "FAIL api.alias") != nullptr);
  CHECK(std::strstr(report, "ok api.copy 65536") != nullptr); }

/* A refusal is the one step reported: nothing it did not make is judged. */
TEST(app_pointer_check, a_refusal_judges_nothing_after_it)
{ CheckRig rig;
  const char *report = rig.judge(static_cast<std::int32_t>(0x80070057), 1u);
  CHECK(std::strstr(report, "FAIL api.heap") != nullptr);
  CHECK(std::strstr(report, "alias") == nullptr);
  CHECK(std::strstr(report, "copy") == nullptr);
  CHECK(std::strstr(report, "avprobe: 0 passed, 1 failed") != nullptr); }

/* A pattern that is not what the check made is a broken check. */
TEST(app_pointer_check_invariants, a_changed_pattern_breaks_them)
{ AppPointerCheck check;
  AppPointerCheckProbe::pattern(check)[AppPointerCheck::bytes - 1u] ^= 1u;
  CHECK(!check.invariants_hold()); }
