#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
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

/* The summary names the program that ran the steps: rvkvkprobe.exe's report
 * is judged by its own line (feature 045). */
TEST(guest_report, the_summary_names_its_program)
{ GuestReport report("rvkvkprobe");
  report.measure("vulkan", "compute", Status::ok, 256u);
  report.finish();
  CHECK(std::strcmp(report.text(), "ok vulkan.compute 256\nrvkvkprobe: 1 passed, 0 failed\n") == 0);
  CHECK(report.invariants_hold()); }

TEST(guest_report_report, names_the_counts_and_the_step_checked_last)
{ GuestReport report("rvkvkprobe");
  char bytes[256];
  av::StateWriter fresh(bytes, sizeof bytes);
  report.report(fresh);
  CHECK(std::strcmp(fresh.dump(),
                    "report/0 program=rvkvkprobe passed=0 failed=0 last_scenario= last_step= finished=false") == 0);
  report.check("adapter", "find", Status::ok, 0u);
  report.measure("transport", "latency", Status::invalid_state, 7u);
  report.check_refused("allocation", "refuse_hostile", Status::ok, 0u);
  av::StateWriter checked(bytes, sizeof bytes);
  report.report(checked);
  CHECK(std::strcmp(checked.dump(), "report/0 program=rvkvkprobe passed=1 failed=2 last_scenario=allocation "
                                    "last_step=refuse_hostile finished=false") == 0);
  report.measure("vsync", "rate", Status::ok, 60u);
  report.finish();
  av::StateWriter done(bytes, sizeof bytes);
  report.report(done);
  CHECK(std::strcmp(done.dump(), "report/0 program=rvkvkprobe passed=2 failed=2 last_scenario=vsync "
                                 "last_step=rate finished=true") == 0);
  CHECK(report.invariants_hold()); }

/* A note carries text a step learned, cut at its first line break or at
 * note_bytes, and counts as neither a pass nor a failure. */
TEST(guest_report, a_note_is_one_line_of_text_and_no_verdict)
{ GuestReport report("rvkglprobe");
  report.note("opengl", "renderer", "Mesa\r\nsecond");
  report.note("opengl", "version", nullptr);
  char long_text[GuestReport::note_bytes + 8u];
  std::memset(long_text, 'x', sizeof long_text - 1u);
  long_text[sizeof long_text - 1u] = '\0';
  report.note("opengl", "long", long_text);
  report.finish();
  char expected[GuestReport::note_bytes + 128u] = "note opengl.renderer Mesa\nnote opengl.version (none)\nnote opengl.long ";
  std::memset(expected + std::strlen(expected), 'x', GuestReport::note_bytes);
  std::strcat(expected, "\nrvkglprobe: 0 passed, 0 failed\n");
  CHECK(std::strcmp(report.text(), expected) == 0);
  CHECK_EQ(report.passed(), 0u);
  CHECK_EQ(report.failed(), 0u);
  CHECK(report.invariants_hold()); }
