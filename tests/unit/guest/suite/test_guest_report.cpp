#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/suite/guest_report.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::GuestReport;
using av::Status;

namespace av {

struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};

struct GuestReportProbe {
    static void break_the_text(GuestReport &report) { GrowableArrayProbe::corrupt_capacity(report.text_, 3u); }
    static void set(GuestReport &report, std::uint32_t index, char value) { report.text_.at(index) = value; }
};

}

TEST(guest_report, writes_a_line_per_step_and_a_summary)
{ av::MockPlatform report_memory;
  GuestReport report(report_memory);
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
                    "avtest: 2 passed, 2 failed\n") == 0);
  CHECK_EQ(report.passed(), 2u);
  CHECK_EQ(report.failed(), 2u);
  CHECK_EQ(report.length(), std::strlen(report.text()));
  CHECK(report.invariants_hold()); }

TEST(guest_report, keeps_every_line_and_the_summary_past_the_old_capacity)
{ av::MockPlatform report_memory;
  GuestReport report(report_memory);
  for (unsigned step = 0u; step < 2000u; step++) report.check("scenario", "a_step_with_a_long_name", Status::ok, 0u);
  report.finish();
  const std::size_t line = std::strlen("ok scenario.a_step_with_a_long_name\n");
  CHECK_EQ(report.length(), 2000u * line + std::strlen("avtest: 2000 passed, 0 failed\n"));
  CHECK_EQ(report.length(), std::strlen(report.text()));
  CHECK(report.length() > 16384u);
  CHECK_EQ(report.passed(), 2000u);
  CHECK(std::strstr(report.text(), "avtest: 2000 passed, 0 failed\n") != nullptr);
  CHECK(report.invariants_hold()); }

TEST(guest_report, a_line_with_no_memory_to_keep_it_is_lost_whole_and_still_counted)
{ av::MockPlatform report_memory;
  GuestReport report(report_memory);
  report.check("adapter", "find", Status::ok, 0u);
  for (unsigned step = 0u; step < 3u; step++) report.check("adapter", "pad", Status::ok, 0u);
  report_memory.fail_allocation_at(1u);
  report.check("submission", "lost", Status::invalid_state, 1u);
  CHECK(report.invariants_hold());
  report.finish();
  CHECK(std::strcmp(report.text(), "ok adapter.find\nok adapter.pad\nok adapter.pad\nok adapter.pad\n"
                                   "avtest: 4 passed, 1 failed\n") == 0);
  CHECK_EQ(report.failed(), 1u);
  CHECK(report.invariants_hold()); }

TEST(guest_report_invariants, fail_for_a_terminator_inside_the_text_or_none_at_its_end)
{ av::MockPlatform report_memory;
  GuestReport report(report_memory);
  CHECK(std::strcmp(report.text(), "") == 0);
  CHECK_EQ(report.length(), 0u);
  report.check("adapter", "find", Status::ok, 0u);
  CHECK(report.invariants_hold());
  av::GuestReportProbe::set(report, 2u, '\0');
  CHECK(!report.invariants_hold());
  av::GuestReportProbe::set(report, 2u, 'a');
  CHECK(report.invariants_hold());
  av::GuestReportProbe::set(report, 16u, 'x');
  CHECK(!report.invariants_hold());
  av::GuestReportProbe::set(report, 16u, '\0');
  CHECK(report.invariants_hold());
  av::GuestReportProbe::break_the_text(report);
  CHECK(!report.invariants_hold()); }

TEST(guest_report, the_summary_names_its_program)
{ av::MockPlatform report_memory;
  GuestReport report(report_memory, "avvkprobe");
  report.measure("vulkan", "compute", Status::ok, 256u);
  report.finish();
  CHECK(std::strcmp(report.text(), "ok vulkan.compute 256\navvkprobe: 1 passed, 0 failed\n") == 0);
  CHECK(report.invariants_hold()); }

TEST(guest_report_report, names_the_counts_and_the_step_checked_last)
{ av::MockPlatform report_memory;
  GuestReport report(report_memory, "avvkprobe");
  char bytes[256];
  av::StateWriter fresh(bytes, sizeof bytes);
  report.report(fresh);
  CHECK(std::strcmp(fresh.dump(),
                    "report/0 program=avvkprobe passed=0 failed=0 last_scenario= last_step= finished=false") == 0);
  report.check("adapter", "find", Status::ok, 0u);
  report.measure("transport", "latency", Status::invalid_state, 7u);
  report.check_refused("allocation", "refuse_hostile", Status::ok, 0u);
  av::StateWriter checked(bytes, sizeof bytes);
  report.report(checked);
  CHECK(std::strcmp(checked.dump(), "report/0 program=avvkprobe passed=1 failed=2 last_scenario=allocation "
                                    "last_step=refuse_hostile finished=false") == 0);
  report.measure("vsync", "rate", Status::ok, 60u);
  report.finish();
  av::StateWriter done(bytes, sizeof bytes);
  report.report(done);
  CHECK(std::strcmp(done.dump(), "report/0 program=avvkprobe passed=2 failed=2 last_scenario=vsync "
                                 "last_step=rate finished=true") == 0);
  CHECK(report.invariants_hold()); }

TEST(guest_report, a_note_is_one_line_of_text_and_no_verdict)
{ av::MockPlatform report_memory;
  GuestReport report(report_memory, "avglprobe");
  report.note("opengl", "renderer", "Mesa\r\nsecond");
  report.note("opengl", "version", nullptr);
  report.note("opengl", "lines", "one\ntwo");
  char long_text[GuestReport::note_bytes + 8u];
  std::memset(long_text, 'x', sizeof long_text - 1u);
  long_text[sizeof long_text - 1u] = '\0';
  report.note("opengl", "long", long_text);
  report.finish();
  char expected[GuestReport::note_bytes + 128u] = "note opengl.renderer Mesa\nnote opengl.version (none)\nnote opengl.lines one\n"
                                                     "note opengl.long ";
  std::memset(expected + std::strlen(expected), 'x', GuestReport::note_bytes);
  std::strcat(expected, "\navglprobe: 0 passed, 0 failed\n");
  CHECK(std::strcmp(report.text(), expected) == 0);
  CHECK_EQ(report.passed(), 0u);
  CHECK_EQ(report.failed(), 0u);
  CHECK(report.invariants_hold()); }
