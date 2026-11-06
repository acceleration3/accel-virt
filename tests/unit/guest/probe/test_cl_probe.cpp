#include "guest/probe/cl_probe.hpp"
#include "guest/suite/guest_report.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_cl.hpp"

#include <cstdint>
#include <cstring>

using av::ClProbe;
using av::GuestReport;
using av::MockKmt;
using av::ScriptedCl;
using Call = av::ScriptedCl::Call;

namespace {
class LateThread final : public av::ClockReader {
public:
    LateThread(ScriptedCl &cl, std::uint32_t after) : cl_(cl), after_(after), reads_(0u) {}

    void clock_read() override
    {
        if (++reads_ != after_) return;
        delivered_ = cl_.deliver_deferred();
    }

    std::uint32_t reads() const { return reads_; }
    bool          delivered() const { return delivered_; }

private:
    ScriptedCl   &cl_;
    std::uint32_t after_;
    std::uint32_t reads_ = 0u;
    bool          delivered_ = false;
};

class ProbeRig {
public:
    static constexpr std::uint64_t clock_step = MockKmt::ticks_per_second / 8u;

    ProbeRig() : report_(report_memory_, "avclprobe") { kmt_.reads_clock(nullptr, clock_step); }
    ~ProbeRig()
    {
        CHECK_EQ(cl_.misuses(), 0u);
        CHECK(report_.invariants_hold());
    }

    ScriptedCl  &cl() { return cl_; }
    MockKmt     &kmt() { return kmt_; }
    GuestReport &report() { return report_; }
    void         run()
    {
        ClProbe probe(kmt_, report_, cl_);
        probe.run();
        report_.finish();
    }
    bool holds(const char *line) const { return std::strstr(report_.text(), line) != nullptr; }

private:
    MockKmt     kmt_;
    ScriptedCl  cl_;
    av::MockPlatform report_memory_;
    GuestReport report_;
};

}

TEST(cl_probe, every_step_passes_on_an_opencl_that_does_what_it_is_asked)
{ ProbeRig rig;
  rig.run();
  CHECK(std::strcmp(rig.report().text(), "ok opencl.platform_count\n"
                                         "ok opencl.platforms\n"
                                         "ok opencl.platform 1\n"
                                         "ok opencl.device_ids\n"
                                         "ok opencl.device 1\n"
                                         "note opencl.device_name scripted avgpu\n"
                                         "ok opencl.luid 4660\n"
                                         "ok opencl.context\n"
                                         "ok opencl.queue\n"
                                         "ok opencl.buffers\n"
                                         "ok opencl.write\n"
                                         "ok opencl.write_second\n"
                                         "ok opencl.program\n"
                                         "ok opencl.build\n"
                                         "ok opencl.kernel\n"
                                         "ok opencl.arguments\n"
                                         "ok opencl.run\n"
                                         "ok opencl.finish\n"
                                         "ok opencl.read\n"
                                         "ok opencl.sum 0\n"
                                         "ok opencl.map 0\n"
                                         "ok opencl.unmap\n"
                                         "ok opencl.svm\n"
                                         "ok opencl.user_event\n"
                                         "ok opencl.wait\n"
                                         "ok opencl.callback 1\n"
                                         "ok opencl.long_program 1600\n"
                                         "ok opencl.long_build\n"
                                         "ok opencl.long_kernel\n"
                                         "ok opencl.long_run\n"
                                         "ok opencl.long_sum 0\n"
                                         "ok opencl.binary 16\n"
                                         "ok opencl.binary_build\n"
                                         "ok opencl.binary_sum 0\n"
                                         "ok opencl.binary_refused 4294967254\n"
                                         "ok opencl.compile\n"
                                         "ok opencl.link\n"
                                         "ok opencl.link_sum 0\n"
                                         "note opencl.il_version SPIR-V_1.0 SPIR-V_1.1\n"
                                         "ok opencl.il\n"
                                         "ok opencl.il_sum 0\n"
                                         "ok opencl.release\n"
                                         "avclprobe: 40 passed, 0 failed\n") == 0);
  CHECK(rig.cl().program_bytes() > 0u);
  CHECK_EQ(rig.cl().calls(Call::run_kernel), 5u);
  CHECK_EQ(rig.cl().calls(Call::write_buffer), 6u);
  CHECK_EQ(rig.cl().calls(Call::release_buffer), 3u);
  CHECK_EQ(rig.cl().calls(Call::release_kernel), 5u);
  CHECK_EQ(rig.cl().calls(Call::release_program), 8u);
  CHECK_EQ(rig.cl().calls(Call::release_queue), 1u);
  CHECK_EQ(rig.cl().calls(Call::release_event), 1u);
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(cl_probe, the_platform_taken_is_the_one_whose_icd_suffix_is_av)
{ ProbeRig rig;
  rig.cl().set_platform_suffix(0u, "INTEL");
  rig.cl().add_platform("AMD");
  rig.cl().add_platform("AV");
  rig.run();
  CHECK(rig.holds("ok opencl.platform 3\n"));
  CHECK(std::strcmp(rig.cl().asked_devices_of(), "AV") == 0);
  CHECK(rig.holds("ok opencl.sum 0\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(cl_probe, no_platform_with_the_av_suffix_ends_the_run)
{ const char *const suffixes[] = {"INTEL", "AVX", "av", "RV", ""};
  for (const char *const suffix : suffixes) {
    ProbeRig rig;
    rig.cl().set_platform_suffix(0u, suffix);
    rig.run();
    CHECK(std::strcmp(rig.report().text(), "ok opencl.platform_count\n"
                                           "ok opencl.platforms\n"
                                           "FAIL opencl.platform status=3 1\n"
                                            "ok opencl.release\n"
                                           "avclprobe: 3 passed, 1 failed\n") == 0);
    CHECK_EQ(rig.cl().calls(Call::create_context), 0u);
  }
  ProbeRig found;
  found.cl().set_platform_suffix(0u, "AVX");
  found.cl().add_platform("AV");
  found.run();
  CHECK(found.holds("ok opencl.platform 2\n")); }

TEST(cl_probe, a_device_with_no_luid_or_no_vendor_fails_the_luid_step)
{ const struct {
      std::uint64_t luid;
      std::uint64_t vendor;
      const char   *line;
  } cases[] = {
      {ScriptedCl::default_luid, ScriptedCl::default_vendor, "ok opencl.luid 4660\n"},
      {0u, ScriptedCl::default_vendor, "FAIL opencl.luid status=3 0\n"},
      {ScriptedCl::default_luid, 0u, "FAIL opencl.luid status=3 4660\n"},
      {1u, 1u, "ok opencl.luid 1\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    rig.cl().set_luid(row.luid);
    rig.cl().set_vendor(row.vendor);
    rig.run();
    CHECK(rig.holds(row.line));
    CHECK(rig.holds("ok opencl.sum 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u);
  }
  ProbeRig refused;
  refused.cl().fail_at(Call::device_number, 1u, ScriptedCl::refused);
  refused.run();
  ProbeRig nameless;
  nameless.cl().fail_at(Call::device_number, 2u, ScriptedCl::refused);
  nameless.run();
  CHECK(nameless.holds("FAIL opencl.luid status=3 0\n"));
  CHECK(refused.holds("FAIL opencl.luid status=3 4660\n")); }

TEST(cl_probe, a_build_that_failed_reports_what_the_compiler_said)
{ ProbeRig rig;
  rig.cl().set_build_log("vector_add.cl:5:9: error: no matching function\nand a second line");
  rig.cl().fail_at(Call::build_program, 1u, -11);
  rig.run();
  CHECK(rig.holds("note opencl.build_log vector_add.cl:5:9: error: no matching function\n"));
  CHECK(rig.holds("FAIL opencl.build status=7 4294967285\n"));
  CHECK(rig.holds("avclprobe: 13 passed, 1 failed\n"));
  CHECK_EQ(rig.cl().calls(Call::create_kernel), 0u);
  ProbeRig silent;
  silent.cl().fail_at(Call::build_program, 1u, -11);
  silent.cl().fail_at(Call::program_build_text, 1u, ScriptedCl::refused);
  silent.run();
  CHECK(!silent.holds("note opencl.build_log"));
  CHECK(silent.holds("FAIL opencl.build status=7 4294967285\n")); }

TEST(cl_probe, a_kernel_that_computes_the_wrong_answer_fails_the_sum_step_by_the_count)
{ const struct {
      std::uint32_t wrong;
      const char   *line;
  } cases[] = {
      {0u, "ok opencl.sum 0\n"},
      {1u, "FAIL opencl.sum status=4 1\n"},
      {7u, "FAIL opencl.sum status=4 7\n"},
      {ClProbe::elements, "FAIL opencl.sum status=4 64\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    rig.cl().miscompute(row.wrong);
    rig.run();
    CHECK(rig.holds(row.line));
    CHECK(rig.holds("ok opencl.read\n"));
    CHECK_EQ(rig.holds("ok opencl.map 0\n"), row.wrong == 0u);
    CHECK(rig.holds("ok opencl.release\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u);
  }
  ProbeRig stale;
  stale.cl().stale_mappings(5u);
  stale.run();
  CHECK(stale.holds("ok opencl.sum 0\n"));
  CHECK(stale.holds("FAIL opencl.map status=4 5\n"));
  CHECK(stale.holds("ok opencl.unmap\n"));
  CHECK(stale.holds("ok opencl.release\n"));
  CHECK_EQ(stale.cl().live_objects(), 0u); }

TEST(cl_probe, a_device_that_offers_no_shared_memory_is_noted_rather_than_failed)
{ ProbeRig rig;
  rig.cl().offer_no_svm();
  rig.run();
  CHECK(rig.holds("note opencl.svm not offered\n"));
  CHECK(!rig.holds("ok opencl.svm\n"));
  CHECK(!rig.holds("FAIL opencl.svm"));
  CHECK_EQ(rig.cl().calls(Call::set_kernel_svm), 0u);
  CHECK(rig.holds("avclprobe: 39 passed, 0 failed\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u);
  ProbeRig refused_allocation;
  refused_allocation.cl().fail_at(Call::svm_alloc, 1u, ScriptedCl::refused);
  refused_allocation.run();
  CHECK(refused_allocation.holds("FAIL opencl.svm status=7 4294967295\n"));
  CHECK_EQ(refused_allocation.cl().live_objects(), 0u);
  ProbeRig refused_argument;
  refused_argument.cl().fail_at(Call::set_kernel_svm, 1u, ScriptedCl::refused);
  refused_argument.run();
  CHECK(refused_argument.holds("FAIL opencl.svm status=7 4294967266\n"));
  CHECK_EQ(refused_argument.cl().live_objects(), 0u); }

TEST(cl_probe, the_callback_runs_once_on_the_event_the_run_completes)
{ ProbeRig rig;
  rig.run();
  CHECK(rig.holds("ok opencl.user_event\n"));
  CHECK(rig.holds("ok opencl.wait\n"));
  CHECK(rig.holds("ok opencl.callback 1\n"));
  CHECK_EQ(rig.cl().calls(Call::create_user_event), 1u);
  CHECK_EQ(rig.cl().calls(Call::set_user_event_status), 1u);
  CHECK_EQ(rig.cl().calls(Call::wait_for_event), 1u);
  ProbeRig never;
  never.cl().forget_callbacks();
  never.run();
  CHECK(never.holds("FAIL opencl.callback status=4 0\n"));
  ProbeRig late;
  late.cl().defer_callbacks();
  LateThread thread(late.cl(), 4u);
  late.kmt().reads_clock(&thread, ProbeRig::clock_step);
  late.run();
  CHECK(thread.delivered());
  CHECK_EQ(thread.reads(), 4u);
  CHECK(late.holds("ok opencl.callback 1\n"));
  ProbeRig twice;
  twice.cl().run_callbacks_twice();
  twice.run();
  CHECK(twice.holds("FAIL opencl.callback status=4 2\n"));
  ProbeRig unwatched;
  unwatched.cl().fail_at(Call::watch_event, 1u, ScriptedCl::refused);
  unwatched.run();
  CHECK(unwatched.holds("FAIL opencl.callback status=7 4294967295\n"));
  CHECK_EQ(unwatched.cl().calls(Call::set_user_event_status), 0u);
  CHECK_EQ(unwatched.cl().calls(Call::release_event), 1u);
  CHECK_EQ(unwatched.cl().live_objects(), 0u); }

TEST(cl_probe, a_step_that_failed_after_the_kernel_still_gives_every_object_back)
{ const struct {
      Call          call;
      std::uint32_t nth;
      const char   *line;
  } cases[] = {
      {Call::run_kernel, 1u, "FAIL opencl.run status=7 4294967266\n"},
      {Call::finish, 1u, "FAIL opencl.finish status=7 4294967266\n"},
      {Call::read_buffer, 1u, "FAIL opencl.read status=7 4294967266\n"},
      {Call::map_buffer, 1u, "FAIL opencl.map status=7 4294967266\n"},
      {Call::unmap, 1u, "FAIL opencl.unmap status=7 4294967266\n"},
      {Call::set_user_event_status, 1u, "FAIL opencl.user_event status=7 4294967266\n"},
      {Call::wait_for_event, 1u, "FAIL opencl.wait status=7 4294967266\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    rig.cl().fail_at(row.call, row.nth, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds(row.line));
    CHECK(rig.holds("ok opencl.release\n"));
    CHECK_EQ(rig.cl().calls(Call::release_context), 1u);
    CHECK_EQ(rig.cl().calls(Call::release_buffer), 3u);
    CHECK_EQ(rig.cl().live_objects(), 0u);
  } }

TEST(cl_probe, a_failure_at_any_call_of_the_run_is_reported_where_it_happened)
{ const struct {
      Call          call;
      std::uint32_t nth;
      const char   *line;
      const char   *next;
  } cases[] = {
      {Call::devices, 1u, "FAIL opencl.device_ids status=7 4294967266\n", "ok opencl.device"},
      {Call::create_context, 1u, "FAIL opencl.context status=7 4294967266\n", "ok opencl.queue\n"},
      {Call::create_queue, 1u, "FAIL opencl.queue status=7 4294967266\n", "ok opencl.buffers\n"},
      {Call::create_buffer, 3u, "FAIL opencl.buffers status=7 4294967266\n", "ok opencl.write\n"},
      {Call::write_buffer, 1u, "FAIL opencl.write status=7 4294967266\n", "ok opencl.write_second\n"},
      {Call::write_buffer, 2u, "FAIL opencl.write_second status=7 4294967266\n", "ok opencl.program\n"},
      {Call::create_program, 1u, "FAIL opencl.program status=7 4294967266\n", "ok opencl.build\n"},
      {Call::create_kernel, 1u, "FAIL opencl.kernel status=7 4294967266\n", "ok opencl.arguments\n"},
      {Call::set_kernel_buffer, 2u, "FAIL opencl.arguments status=7 4294967266\n", "ok opencl.run\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    rig.cl().fail_at(row.call, row.nth, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds(row.line));
    CHECK(!rig.holds(row.next));
  }
  ProbeRig counted;
  counted.cl().fail_at(Call::platform_count, 1u, ScriptedCl::refused);
  counted.run();
  CHECK(std::strcmp(counted.report().text(), "FAIL opencl.platform_count status=7 4294967266\n"
                                            "ok opencl.release\n"
                                             "avclprobe: 1 passed, 1 failed\n") == 0);
  ProbeRig listed;
  listed.cl().fail_at(Call::platforms, 1u, ScriptedCl::refused);
  listed.run();
  CHECK(std::strcmp(listed.report().text(), "ok opencl.platform_count\n"
                                            "FAIL opencl.platforms status=7 4294967266\n"
                                            "ok opencl.release\n"
                                            "avclprobe: 2 passed, 1 failed\n") == 0);
  ProbeRig silent;
  silent.cl().add_platform("AV");
  silent.cl().fail_at(Call::platform_text, 1u, ScriptedCl::refused);
  silent.run();
  CHECK(silent.holds("ok opencl.platform 2\n"));
  ProbeRig nameless;
  nameless.cl().fail_at(Call::device_text, 1u, ScriptedCl::refused);
  nameless.run();
  CHECK(!nameless.holds("note opencl.device_name"));
  CHECK(nameless.holds("ok opencl.device 1\n"));
  ProbeRig empty;
  empty.cl().set_device_count(0u);
  empty.run();
  CHECK(empty.holds("FAIL opencl.device status=3 0\n"));
  CHECK_EQ(empty.cl().calls(Call::create_context), 0u); }

TEST(cl_probe, what_the_icds_thread_calls_counts_deliveries_rather_than_the_last_one)
{ MockKmt     kmt;
  ScriptedCl  cl;
  av::MockPlatform report_memory;
  GuestReport report(report_memory, "avclprobe");
  ClProbe     probe(kmt, report, cl);
  probe.callback_arrived(0);
  probe.callback_arrived(-5);
  probe.run();
  report.finish();
  CHECK(std::strstr(report.text(), "ok opencl.callback 1\n") != nullptr); }

TEST(cl_probe, a_platform_whose_suffix_is_not_av_is_not_this_driver)
{ ProbeRig rig;
  rig.cl().set_platform_suffix(0u, "RXY");
  rig.cl().add_platform("RVX");
  rig.cl().add_platform("AVX");
  rig.run();
  CHECK(rig.holds("FAIL opencl.platform status=3 3\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u);

  ProbeRig many;
  many.cl().set_platform_suffix(0u, "RXY");
  for (std::uint32_t index = 1u; index < 10u; ++index) many.cl().add_platform("RXY");
  many.cl().overstate_platforms();
  many.run();
  CHECK(many.holds("FAIL opencl.platform status=3 10\n"));
  CHECK_EQ(many.cl().live_objects(), 0u);

  ProbeRig among;
  among.cl().set_platform_suffix(0u, "RXY");
  among.cl().add_platform("RVX");
  among.cl().add_platform("AV");
  among.run();
  CHECK(among.holds("ok opencl.platform 3\n"));
  CHECK_EQ(among.cl().live_objects(), 0u); }

TEST(cl_probe, a_platform_that_will_not_say_what_it_is_is_skipped)
{ ProbeRig rig;
  rig.cl().set_platform_suffix(0u, "RXY");
  rig.cl().add_platform("AV");
  rig.cl().fail_at(Call::platform_text, 1u, ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("ok opencl.platform 2\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(cl_probe, the_first_buffer_refused_is_the_one_the_step_reports)
{ ProbeRig rig;
  rig.cl().fail_from(Call::create_buffer, 2u, -61 );
  rig.run();
  CHECK(rig.holds("FAIL opencl.buffers status=7 4294967235\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u);

  ProbeRig silent;
  silent.cl().fail_silently_at(Call::create_buffer, 1u);
  silent.run();
  CHECK(silent.holds("FAIL opencl.buffers status=7 4294967295\n"));
  CHECK_EQ(silent.cl().live_objects(), 0u); }

TEST(cl_probe, a_device_that_will_not_answer_about_shared_memory_notes_it_too)
{ ProbeRig rig;
  rig.cl().fail_at(Call::device_number, 3u, ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("note opencl.svm not offered\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(cl_probe, a_map_or_a_user_event_that_is_refused_says_so)
{ { ProbeRig rig;
    rig.cl().fail_at(Call::map_buffer, 1u, -12 );
    rig.run();
    CHECK(rig.holds("FAIL opencl.map status=7 4294967284\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_silently_at(Call::map_buffer, 1u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.map status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_user_event, 1u, -6 );
    rig.run();
    CHECK(rig.holds("FAIL opencl.callback status=7 4294967290\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_silently_at(Call::create_user_event, 1u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.callback status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, a_long_program_the_opencl_refuses_fails_its_step_and_releases_everything)
{ ProbeRig rig;
  rig.cl().fail_at(Call::create_program, 2u, -5);
  rig.run();
  CHECK(rig.holds("ok opencl.sum 0\n"));
  CHECK(rig.holds("FAIL opencl.long_program status=7 4294967291\n"));
  CHECK(!rig.holds("opencl.long_build"));
  CHECK(rig.holds("ok opencl.release\n"));
  CHECK_EQ(rig.cl().calls(Call::run_kernel), 4u);
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(cl_probe, a_binary_that_cannot_be_had_or_made_into_a_program_fails_its_step)
{ { ProbeRig rig;
    rig.cl().fail_at(Call::program_binary, 1u, -30);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary status=7 4294967266\n"));
    CHECK(!rig.holds("opencl.binary_build"));
    CHECK(rig.holds("ok opencl.link_sum 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_program_with_binary, 1u, -42);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary_build status=7 4294967254\n"));
    CHECK(!rig.holds("opencl.binary_sum"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_program_with_binary, 2u, 0);
    rig.cl().fail_silently_at(Call::create_program_with_binary, 2u);
    rig.run();
    CHECK(rig.holds("ok opencl.binary_sum 0\n"));
    CHECK(rig.holds("FAIL opencl.binary_refused"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::run_kernel, 3u, -5);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary_sum status=4 4294967291\n"));
    CHECK(rig.holds("ok opencl.link_sum 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, a_compile_or_link_the_opencl_refuses_fails_its_step)
{ { ProbeRig rig;
    rig.cl().fail_at(Call::compile_program, 2u, -15);
    rig.run();
    CHECK(rig.holds("FAIL opencl.compile status=7 4294967281\n"));
    CHECK(!rig.holds("opencl.link "));
    CHECK(rig.holds("ok opencl.il_sum 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::link_program, 1u, -17);
    rig.run();
    CHECK(rig.holds("ok opencl.compile\n"));
    CHECK(rig.holds("FAIL opencl.link status=7 4294967279\n"));
    CHECK(!rig.holds("opencl.link_sum"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_program, 3u, -6);
    rig.run();
    CHECK(rig.holds("FAIL opencl.compile status=7 4294967290\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, a_device_without_il_is_judged_by_its_refusal)
{ { ProbeRig rig;
    rig.cl().offer_no_il();
    rig.run();
    CHECK(rig.holds("note opencl.il_version not offered\n"));
    CHECK(rig.holds("ok opencl.il 4294967237\n"));
    CHECK(!rig.holds("opencl.il_sum"));
    CHECK(rig.holds("avclprobe: 39 passed, 0 failed\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().offer_no_il();
    rig.cl().fail_at(Call::create_program_with_il, 1u, -30);
    rig.run();
    CHECK(rig.holds("FAIL opencl.il status=4 4294967266\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_program_with_il, 1u, -30);
    rig.run();
    CHECK(rig.holds("FAIL opencl.il status=7 4294967266\n"));
    CHECK(!rig.holds("opencl.il_sum"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, a_kernel_that_computes_wrongly_fails_its_own_sum_by_the_count)
{ const struct {
      std::uint32_t runs;
      const char   *line;
  } cases[] = {
      {1u, "FAIL opencl.long_sum status=4 3\n"},
      {2u, "FAIL opencl.binary_sum status=4 3\n"},
      {3u, "FAIL opencl.link_sum status=4 3\n"},
      {4u, "FAIL opencl.il_sum status=4 3\n"},
  };
  for (const auto &row : cases) {
      ProbeRig rig;
      rig.cl().miscompute_after(row.runs, 3u);
      rig.run();
      CHECK(rig.holds("ok opencl.sum 0\n"));
      CHECK(rig.holds(row.line));
      CHECK(rig.holds("ok opencl.release\n"));
      CHECK_EQ(rig.cl().live_objects(), 0u);
  } }

TEST(cl_probe, the_long_program_is_failed_at_the_step_that_refused_it)
{ const struct {
      Call          call;
      std::uint32_t nth;
      const char   *line;
  } cases[] = {
      {Call::build_program, 2u, "FAIL opencl.long_build status=7 4294967266\n"},
      {Call::create_kernel, 2u, "FAIL opencl.long_kernel status=7 4294967266\n"},
      {Call::run_kernel, 2u, "FAIL opencl.long_run status=7 4294967266\n"},
      {Call::set_kernel_buffer, 5u, "FAIL opencl.long_run status=7 4294967266\n"},
      {Call::write_buffer, 3u, "FAIL opencl.long_run status=7 4294967266\n"},
      {Call::finish, 2u, "FAIL opencl.long_run status=7 4294967266\n"},
      {Call::read_buffer, 2u, "FAIL opencl.long_run status=7 4294967266\n"},
  };
  for (const auto &row : cases) {
      ProbeRig rig;
      rig.cl().fail_at(row.call, row.nth, ScriptedCl::refused);
      rig.run();
      CHECK(rig.holds(row.line));
      CHECK(rig.holds("ok opencl.release\n"));
      CHECK_EQ(rig.cl().live_objects(), 0u);
  } }

TEST(cl_probe, a_later_kernel_that_cannot_be_run_is_failed_by_the_error_that_stopped_it)
{ const struct {
      Call          call;
      std::uint32_t nth;
      const char   *line;
  } cases[] = {
      {Call::set_kernel_buffer, 9u, "FAIL opencl.binary_sum status=4 4294967266\n"},
      {Call::run_kernel, 4u, "FAIL opencl.link_sum status=4 4294967266\n"},
      {Call::run_kernel, 5u, "FAIL opencl.il_sum status=4 4294967266\n"},
  };
  for (const auto &row : cases) {
      ProbeRig rig;
      rig.cl().fail_at(row.call, row.nth, ScriptedCl::refused);
      rig.run();
      CHECK(rig.holds(row.line));
      CHECK_EQ(rig.cl().live_objects(), 0u);
  } }

TEST(cl_probe, a_binary_with_no_bytes_or_no_kernel_in_it_fails_its_build)
{ { ProbeRig rig;
    rig.cl().give_binaries_without_bytes();
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary status=7 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_kernel, 3u, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary_build status=7 4294967266\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_silently_at(Call::create_kernel, 3u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary_build status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::build_program, 3u, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary_build status=7 4294967266\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, a_binary_the_opencl_loads_wrongly_or_refuses_for_the_wrong_reason_fails_its_steps)
{ { ProbeRig rig;
    rig.cl().refuse_binary_by_status_at(1u, -42);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary_build status=7 4294967254\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().accept_any_binary();
    rig.run();
    CHECK(rig.holds("ok opencl.binary_sum 0\n"));
    CHECK(rig.holds("FAIL opencl.binary_refused status=4 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_program_with_binary, 2u, -42);
    rig.run();
    CHECK(rig.holds("FAIL opencl.binary_refused status=4 4294967254\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, a_link_the_opencl_makes_no_kernel_of_fails_its_step)
{ { ProbeRig rig;
    rig.cl().fail_at(Call::create_kernel, 4u, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds("FAIL opencl.link status=7 4294967266\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_silently_at(Call::create_kernel, 4u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.link status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, il_is_found_in_a_version_list_whatever_stands_before_it)
{ const char *const lists[] = {"SX SPX SPIX SPIRX SPIR-X SPIR-V_1.2", "SPIR-V_1.2"};
  for (const char *const text : lists) {
      ProbeRig rig;
      rig.cl().offer_il_versions(text);
      rig.run();
      CHECK(rig.holds("ok opencl.il\n"));
      CHECK(rig.holds("ok opencl.il_sum 0\n"));
      CHECK_EQ(rig.cl().live_objects(), 0u);
  } }

TEST(cl_probe, a_version_list_that_fills_the_buffer_without_il_or_will_not_be_read_offers_none)
{ { ProbeRig rig;
    char text[256];
    std::memset(text, 'x', sizeof text - 1u);
    text[sizeof text - 1u] = '\0';
    rig.cl().offer_il_versions(text);
    rig.run();
    CHECK(rig.holds("note opencl.il_version not offered\n"));
    CHECK(rig.holds("FAIL opencl.il status=4 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::device_text, 2u, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds("note opencl.il_version not offered\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }

TEST(cl_probe, an_il_the_opencl_makes_no_kernel_of_fails_its_step)
{ { ProbeRig rig;
    rig.cl().fail_at(Call::build_program, 4u, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds("FAIL opencl.il status=7 4294967266\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_kernel, 5u, ScriptedCl::refused);
    rig.run();
    CHECK(rig.holds("FAIL opencl.il status=7 4294967266\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_silently_at(Call::create_kernel, 5u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.il status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_silently_at(Call::create_program_with_il, 1u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.il status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }
