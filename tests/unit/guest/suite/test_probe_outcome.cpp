/* How one of the probe's workloads went, and the report line it becomes: the
 * first failure is the one kept, whatever follows it; a missing type is its own
 * failure; a count passes only when every value was right. */
#include "guest/suite/guest_report.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstring>

namespace av {

struct ProbeOutcomeProbe {
    static void failed_and_missing(ProbeOutcome &outcome) { outcome.result_ = VK_ERROR_UNKNOWN; outcome.missing_ = true; }
    static void over_counted(ProbeOutcome &outcome) { outcome.matched_ = 2u; outcome.expected_ = 1u; }
};

}  // namespace av

using av::GuestReport;
using av::MockKmt;
using av::ProbeOutcome;
using av::ProbeOutcomeProbe;
using av::ScenarioSteps;

namespace {

class OutcomeRig {
public:
    OutcomeRig() : steps_(kmt_, report_, "vulkan") {}

    const char *line(const ProbeOutcome &outcome)
    {
        outcome.report(steps_, "work");
        return report_.text();
    }

private:
    MockKmt       kmt_;
    GuestReport   report_;
    ScenarioSteps steps_;
};

}  // namespace

TEST(probe_outcome, every_value_right_passes_with_its_count)
{ OutcomeRig rig;
  ProbeOutcome outcome;
  CHECK(outcome.accept(VK_SUCCESS));
  outcome.measure(256u, 256u);
  CHECK(outcome.invariants_hold());
  CHECK(std::strcmp(rig.line(outcome), "ok vulkan.work 256\n") == 0); }

TEST(probe_outcome, one_value_short_fails_with_its_count)
{ OutcomeRig rig;
  ProbeOutcome outcome;
  outcome.measure(255u, 256u);
  CHECK(outcome.invariants_hold());
  CHECK(std::strcmp(rig.line(outcome), "FAIL vulkan.work status=4 255\n") == 0); }

/* Nothing measured has not passed. */
TEST(probe_outcome, nothing_measured_fails)
{ OutcomeRig rig;
  const ProbeOutcome outcome;
  CHECK(outcome.invariants_hold());
  CHECK(std::strcmp(rig.line(outcome), "FAIL vulkan.work status=4 0\n") == 0); }

/* A failed call is kept, and a later one or a later missing type does not
 * replace it. */
TEST(probe_outcome, the_first_failed_call_is_the_one_reported)
{ OutcomeRig rig;
  ProbeOutcome outcome;
  CHECK(!outcome.accept(VK_TIMEOUT));
  CHECK(outcome.invariants_hold());
  CHECK(!outcome.accept(VK_ERROR_DEVICE_LOST));
  outcome.missing();
  outcome.measure(1u, 1u);
  CHECK(outcome.invariants_hold());
  CHECK(std::strcmp(rig.line(outcome), "FAIL vulkan.work status=7 2\n") == 0); }

/* A missing type is kept over a failure after it. */
TEST(probe_outcome, a_missing_type_is_not_replaced_by_a_later_failure)
{ OutcomeRig rig;
  ProbeOutcome outcome;
  outcome.missing();
  CHECK(outcome.invariants_hold());
  CHECK(!outcome.accept(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK(outcome.invariants_hold());
  CHECK(std::strcmp(rig.line(outcome), "FAIL vulkan.work status=3 0\n") == 0); }

TEST(probe_outcome, invariants_refuse_two_failures_and_an_over_count)
{ ProbeOutcome both;
  ProbeOutcomeProbe::failed_and_missing(both);
  CHECK(!both.invariants_hold());
  ProbeOutcome over;
  ProbeOutcomeProbe::over_counted(over);
  CHECK(!over.invariants_hold()); }
