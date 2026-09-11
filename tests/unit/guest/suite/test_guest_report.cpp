#include "core/support/status.hpp"
#include "guest/suite/guest_report.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstring>

using av::GuestReport;
using av::Status;

TEST(guest_report, writes_a_line_per_step_and_a_summary)
{ GuestReport report;
  CHECK_EQ(report.length(), 0u);
  CHECK(report.invariants_hold());
  report.check("adapter", "find", Status::ok, 0u);
  report.check("submission", "wait", Status::invalid_state, 0xC00000B5u);
  report.check_refused("allocation", "refuse_hostile", Status::invalid_parameter, 0xC000000Du);
  report.check_refused("allocation", "accepted_hostile", Status::ok, 0u);
  CHECK(report.invariants_hold());
  report.finish();
  CHECK(std::strcmp(report.text(),
                    "ok adapter.find\n"
                    "FAIL submission.wait status=7 code=0xC00000B5\n"
                    "ok allocation.refuse_hostile\n"
                    "FAIL allocation.accepted_hostile status=0 code=0x00000000\n"
                    "rvktest: 2 passed, 2 failed\n") == 0);
  CHECK_EQ(report.passed(), 2u);
  CHECK_EQ(report.failed(), 2u);
  CHECK_EQ(report.length(), std::strlen(report.text()));
  CHECK(report.invariants_hold()); }

TEST(guest_report, text_past_the_capacity_is_cut_but_every_step_is_counted)
{ GuestReport report;
  for (unsigned step = 0u; step < 2000u; step++) report.check("scenario", "a_step_with_a_long_name", Status::ok, 0u);
  report.finish();
  CHECK_EQ(report.length(), GuestReport::capacity - 1u);
  CHECK_EQ(report.passed(), 2000u);
  CHECK(std::strstr(report.text(), "rvktest:") == nullptr);
  CHECK(report.invariants_hold()); }
