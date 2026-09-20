/* The CL probe over a scripted OpenCL (feature 052, T029): every step's line
 * as tools/guest/run_guest_suite.py reads it, the platform picked out of the
 * ones a loader lists by its RVK suffix, the device's LUID, the compiler's log
 * when a build fails, and a vector add read back and compared with what the
 * inputs add to - the scripted ClEntry really adds them, so the sum step
 * judges arithmetic rather than itself. A failure at each call is reported and
 * stops the run where nothing past it can run, and a run that got as far as
 * the kernel gives every object back.
 *
 * ClProbe::run only calls release_all when the read step fails: a failure
 * before it (a context, a queue, a buffer, a build, a kernel) returns with the
 * objects it made still live, so only the failures after the kernel are
 * asserted to release. */
#include "guest/suite/cl_probe.hpp"
#include "guest/suite/guest_report.hpp"
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

/* A step judged by an OpenCL error reports the error's two's-complement bits
 * in decimal where an NTSTATUS would be: 4294967266 is CL_INVALID_VALUE,
 * 4294967285 CL_BUILD_PROGRAM_FAILURE, and 4294967295 the -1 the probe reports
 * for a step that failed without an error of its own.
 */

/* The ICD's event thread, played by the clock the probe waits on: the probe
 * spins reading the counter, and the `after`th read is where that thread gets
 * round to the callback. Nothing else in this test is threaded, so this read
 * is the only moment a delivery can be made to land inside the wait. */
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
    /* A clock that moves an eighth of a second every time it is read, so a
     * probe waiting five seconds for a callback that is not coming reaches
     * its deadline in forty reads rather than in real time. */
    static constexpr std::uint64_t clock_step = MockKmt::ticks_per_second / 8u;

    ProbeRig() : report_("rvkclprobe") { kmt_.reads_clock(nullptr, clock_step); }
    ~ProbeRig()
    {
        /* Nothing was released twice, and nothing the run made up. */
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
    GuestReport report_;
};

}  // namespace

TEST(cl_probe, every_step_passes_on_an_opencl_that_does_what_it_is_asked)
{ ProbeRig rig;
  rig.run();
  CHECK(std::strcmp(rig.report().text(), "ok opencl.platform_count\n"
                                         "ok opencl.platforms\n"
                                         "ok opencl.platform 1\n"
                                         "ok opencl.device_ids\n"
                                         "ok opencl.device 1\n"
                                         "note opencl.device_name scripted rvkgpu\n"
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
                                         "ok opencl.release\n"
                                         "rvkclprobe: 26 passed, 0 failed\n") == 0);
  /* One dispatch of the vector add, the three buffers it ran over given back,
   * and the shared allocation freed. */
  CHECK_EQ(rig.cl().calls(Call::run_kernel), 1u);
  CHECK_EQ(rig.cl().calls(Call::write_buffer), 2u);
  CHECK_EQ(rig.cl().calls(Call::release_buffer), 3u);
  CHECK_EQ(rig.cl().calls(Call::release_kernel), 1u);
  CHECK_EQ(rig.cl().calls(Call::release_program), 1u);
  CHECK_EQ(rig.cl().calls(Call::release_queue), 1u);
  CHECK_EQ(rig.cl().calls(Call::release_event), 1u);
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(cl_probe, the_platform_taken_is_the_one_whose_icd_suffix_is_rvk)
{ ProbeRig rig;
  /* What a machine with three vendors' ICDs installed lists, this driver's
   * last. */
  rig.cl().set_platform_suffix(0u, "INTEL");
  rig.cl().add_platform("AMD");
  rig.cl().add_platform("RVK");
  rig.run();
  CHECK(rig.holds("ok opencl.platform 3\n"));
  CHECK(std::strcmp(rig.cl().asked_devices_of(), "RVK") == 0);
  CHECK(rig.holds("ok opencl.sum 0\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(cl_probe, no_platform_with_the_rvk_suffix_ends_the_run)
{ const char *const suffixes[] = {"INTEL", "RVKX", "rvk", "RV", ""};
  for (const char *const suffix : suffixes) {
    ProbeRig rig;
    rig.cl().set_platform_suffix(0u, suffix);
    rig.run();
    CHECK(std::strcmp(rig.report().text(), "ok opencl.platform_count\n"
                                           "ok opencl.platforms\n"
                                           "FAIL opencl.platform status=3 1\n"
                                            "ok opencl.release\n"
                                           "rvkclprobe: 3 passed, 1 failed\n") == 0);
    CHECK_EQ(rig.cl().calls(Call::create_context), 0u);
  }
  /* The suffix among others still ends the search on itself. */
  ProbeRig found;
  found.cl().set_platform_suffix(0u, "RVKX");
  found.cl().add_platform("RVK");
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
    /* The step is not fatal: the run goes on to the kernel either way. */
    CHECK(rig.holds("ok opencl.sum 0\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u);
  }
  /* A device that will not answer either number at all. */
  ProbeRig refused;
  refused.cl().fail_at(Call::device_number, 1u, ScriptedCl::refused);
  refused.run();
  /* And the LUID query itself refused, which is the second number asked
   * for: a device that will not name its adapter is not this adapter's. */
  ProbeRig nameless;
  nameless.cl().fail_at(Call::device_number, 2u, ScriptedCl::refused);
  nameless.run();
  CHECK(nameless.holds("FAIL opencl.luid status=3 0\n"));
  /* The vendor is what it refused; the LUID it did answer is what the line
   * measures, so a reader can see the device was reached at all. */
  CHECK(refused.holds("FAIL opencl.luid status=3 4660\n")); }

TEST(cl_probe, a_build_that_failed_reports_what_the_compiler_said)
{ ProbeRig rig;
  rig.cl().set_build_log("vector_add.cl:5:9: error: no matching function\nand a second line");
  rig.cl().fail_at(Call::build_program, 1u, -11);
  rig.run();
  CHECK(rig.holds("note opencl.build_log vector_add.cl:5:9: error: no matching function\n"));
  CHECK(rig.holds("FAIL opencl.build status=7 4294967285\n"));
  /* Nothing past the build can run. */
  CHECK(rig.holds("rvkclprobe: 13 passed, 1 failed\n"));
  CHECK_EQ(rig.cl().calls(Call::create_kernel), 0u);
  /* A build that failed and a log that cannot be read: the failure is still
   * reported, with no note. */
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
    /* Wrong or not, the read itself succeeded. */
    CHECK(rig.holds("ok opencl.read\n"));
    /* A wrong sum stops the run before the mapping, and gives everything
     * back. */
    CHECK_EQ(rig.holds("ok opencl.map 0\n"), row.wrong == 0u);
    CHECK(rig.holds("ok opencl.release\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u);
  }
  /* A mapping that shows something other than what the read did - stale host
   * bytes - is counted by the map step the same way, and the run goes on: the
   * mapping is not what the probe is for. */
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
  /* Everything else still passes: the run is one step shorter, not failed. */
  CHECK(rig.holds("rvkclprobe: 25 passed, 0 failed\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u);
  /* A device that offers shared memory and then will not allocate it, and one
   * that will not take the pointer as an argument: both fail the step, and
   * neither leaks the allocation. */
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
  /* A callback the ICD's event thread never ran, and one it ran twice. */
  ProbeRig never;
  never.cl().forget_callbacks();
  never.run();
  CHECK(never.holds("FAIL opencl.callback status=4 0\n"));
  /* And one it had not run when the wait on the event returned, which is what
   * a real ICD's thread does whenever the waiting thread gets there first.
   * The probe waits for it rather than judging the race: a run that read the
   * count at the wait would report this as the callback that never came. */
  ProbeRig late;
  late.cl().defer_callbacks();
  LateThread thread(late.cl(), 4u);
  late.kmt().reads_clock(&thread, ProbeRig::clock_step);
  late.run();
  CHECK(thread.delivered());
  /* Four reads: the one that starts the wait, two that found nothing, and the
   * one the delivery rode in on - after which the probe saw the callback and
   * stopped. A probe that judged at the wait would have read the clock never,
   * and one that kept spinning would have read it forty times. */
  CHECK_EQ(thread.reads(), 4u);
  CHECK(late.holds("ok opencl.callback 1\n"));
  ProbeRig twice;
  twice.cl().run_callbacks_twice();
  twice.run();
  CHECK(twice.holds("FAIL opencl.callback status=4 2\n"));
  /* An ICD that will not register one at all: the step fails and the event is
   * given back anyway. */
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
    /* Nothing past the failure ran. */
    CHECK(!rig.holds(row.next));
  }
  /* The two calls before a platform is even known. */
  ProbeRig counted;
  counted.cl().fail_at(Call::platform_count, 1u, ScriptedCl::refused);
  counted.run();
  CHECK(std::strcmp(counted.report().text(), "FAIL opencl.platform_count status=7 4294967266\n"
                                            "ok opencl.release\n"
                                             "rvkclprobe: 1 passed, 1 failed\n") == 0);
  ProbeRig listed;
  listed.cl().fail_at(Call::platforms, 1u, ScriptedCl::refused);
  listed.run();
  CHECK(std::strcmp(listed.report().text(), "ok opencl.platform_count\n"
                                            "FAIL opencl.platforms status=7 4294967266\n"
                                            "ok opencl.release\n"
                                            "rvkclprobe: 2 passed, 1 failed\n") == 0);
  /* A platform whose suffix cannot be read is one the search walks past. */
  ProbeRig silent;
  silent.cl().add_platform("RVK");
  silent.cl().fail_at(Call::platform_text, 1u, ScriptedCl::refused);
  silent.run();
  CHECK(silent.holds("ok opencl.platform 2\n"));
  /* A device that will not name itself is still a device. */
  ProbeRig nameless;
  nameless.cl().fail_at(Call::device_text, 1u, ScriptedCl::refused);
  nameless.run();
  CHECK(!nameless.holds("note opencl.device_name"));
  CHECK(nameless.holds("ok opencl.device 1\n"));
  /* No device at all. */
  ProbeRig empty;
  empty.cl().set_device_count(0u);
  empty.run();
  CHECK(empty.holds("FAIL opencl.device status=3 0\n"));
  CHECK_EQ(empty.cl().calls(Call::create_context), 0u); }

TEST(cl_probe, what_the_icds_thread_calls_counts_deliveries_rather_than_the_last_one)
{ MockKmt     kmt;
  ScriptedCl  cl;
  GuestReport report("rvkclprobe");
  ClProbe     probe(kmt, report, cl);
  /* The entry the ICD's own thread reaches, which is why it is public and why
   * what it writes into is a member: it may be called after the step that
   * registered it has returned, and more than once if the ICD is wrong. */
  probe.callback_arrived(0);
  probe.callback_arrived(-5);
  probe.run();
  report.finish();
  /* The run zeroes the count before registering, so the two above are not
   * counted - and the one real delivery is. */
  CHECK(std::strstr(report.text(), "ok opencl.callback 1\n") != nullptr); }

/* The platform this driver owns is found by its ICD suffix, and a machine may
 * have others: the loader lists them all, and a suffix that merely starts the
 * same way is not this one. */
TEST(cl_probe, a_platform_whose_suffix_is_not_rvk_is_not_this_driver)
{ ProbeRig rig;
  /* One that shares the first letter, one that shares two, and one that is
   * RVK with more after it - none of them is this driver's. */
  rig.cl().set_platform_suffix(0u, "RXY");
  rig.cl().add_platform("RVX");
  rig.cl().add_platform("RVKX");
  rig.run();
  CHECK(rig.holds("FAIL opencl.platform status=3 3\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u);

  /* More platforms than the probe's own walk reads. Its bound is what keeps
   * it inside its array whatever the loader lists, so a machine with more
   * than eight finds nothing rather than reading past. */
  ProbeRig many;
  many.cl().set_platform_suffix(0u, "RXY");
  for (std::uint32_t index = 1u; index < 10u; ++index) many.cl().add_platform("RXY");
  /* And it says it wrote all ten into room for eight. Nothing stops a real
   * loader doing this; the probe's own bound is what keeps it inside its
   * array when one does. */
  many.cl().overstate_platforms();
  many.run();
  CHECK(many.holds("FAIL opencl.platform status=3 10\n"));
  CHECK_EQ(many.cl().live_objects(), 0u);

  /* And the real one, found among them rather than first. */
  ProbeRig among;
  among.cl().set_platform_suffix(0u, "RXY");
  among.cl().add_platform("RVX");
  among.cl().add_platform("RVK");
  among.run();
  CHECK(among.holds("ok opencl.platform 3\n"));
  CHECK_EQ(among.cl().live_objects(), 0u); }

/* A platform the loader will not describe is skipped rather than believed:
 * the probe cannot tell whose it is, so it is not this driver's. */
TEST(cl_probe, a_platform_that_will_not_say_what_it_is_is_skipped)
{ ProbeRig rig;
  rig.cl().set_platform_suffix(0u, "RXY");
  rig.cl().add_platform("RVK");
  /* The first platform's suffix query is refused; the second answers. */
  rig.cl().fail_at(Call::platform_text, 1u, ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("ok opencl.platform 2\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u); }

/* Every buffer the run needs is asked for, and the first refusal is the one
 * reported - a later one must not overwrite it, because the first is where
 * the run actually stopped being possible. */
TEST(cl_probe, the_first_buffer_refused_is_the_one_the_step_reports)
{ ProbeRig rig;
  /* From the second onward: the third fails too, and the first refusal is
   * still the one reported. */
  rig.cl().fail_from(Call::create_buffer, 2u, -61 /* CL_INVALID_BUFFER_SIZE */);
  rig.run();
  CHECK(rig.holds("FAIL opencl.buffers status=7 4294967235\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u);

  /* A buffer refused with no reason given still fails the step: a zero
   * would read as success, so the step makes a refusal of its own. */
  ProbeRig silent;
  silent.cl().fail_silently_at(Call::create_buffer, 1u);
  silent.run();
  CHECK(silent.holds("FAIL opencl.buffers status=7 4294967295\n"));
  CHECK_EQ(silent.cl().live_objects(), 0u); }

/* A device that will not say whether it has shared memory is not a device
 * without it: the query failing and the answer being none are different
 * things, and only one of them is the device saying so. */
TEST(cl_probe, a_device_that_will_not_answer_about_shared_memory_notes_it_too)
{ ProbeRig rig;
  /* The SVM capability query is the third number the run asks for. */
  rig.cl().fail_at(Call::device_number, 3u, ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("note opencl.svm not offered\n"));
  CHECK_EQ(rig.cl().live_objects(), 0u); }

/* The map and the callback, refused at the call that makes them. Each
 * reports what it was told, or a refusal of its own when it was told
 * nothing - a step that answered zero would read as success. */
TEST(cl_probe, a_map_or_a_user_event_that_is_refused_says_so)
{ { ProbeRig rig;
    rig.cl().fail_at(Call::map_buffer, 1u, -12 /* CL_MAP_FAILURE */);
    rig.run();
    CHECK(rig.holds("FAIL opencl.map status=7 4294967284\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    /* Refused with no reason given: the step still has to fail, so it makes
     * one rather than reporting a zero that reads as success. */
    rig.cl().fail_silently_at(Call::map_buffer, 1u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.map status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_at(Call::create_user_event, 1u, -6 /* CL_OUT_OF_HOST_MEMORY */);
    rig.run();
    CHECK(rig.holds("FAIL opencl.callback status=7 4294967290\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); }
  { ProbeRig rig;
    rig.cl().fail_silently_at(Call::create_user_event, 1u);
    rig.run();
    CHECK(rig.holds("FAIL opencl.callback status=7 4294967295\n"));
    CHECK_EQ(rig.cl().live_objects(), 0u); } }
