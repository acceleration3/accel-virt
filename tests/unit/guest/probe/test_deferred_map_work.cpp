#include "guest/probe/deferred_map_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d11_deferred.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct DeferredMapWorkProbe {
    static void drew_without_a_device(DeferredMapWork &work) { work.drawn_ = 1u; }
    static void grew_without_a_device(DeferredMapWork &work) { work.growth_ = 1u; }
    static std::uint32_t matching(const DeferredMapWork &work) { return work.matching(); }
    static void drew_past_the_frames(DeferredMapWork &work)
    {
        work.made_ = true;
        work.drawn_ = DeferredMapWork::warm_frames + DeferredMapWork::measured_frames + 1u;
    }
};

}

using av::DeferredMapWork;
using av::DeferredMapWorkProbe;
using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;
using av::ScriptedD3d11Deferred;
using Call = av::ScriptedD3d11Deferred::Call;

namespace {

void *const window = reinterpret_cast<void *>(0x9011u);
constexpr std::uint32_t frames = DeferredMapWork::warm_frames + DeferredMapWork::measured_frames;
constexpr std::uint32_t first_measured = DeferredMapWork::warm_frames + 1u;

class DeferredRig {
public:
    DeferredRig() : report_(report_memory_, "avd3d11probe"), steps_(kmt_, report_, "d3d11") {}
    DeferredRig(const DeferredRig &) = delete;
    DeferredRig &operator=(const DeferredRig &) = delete;

    ScriptedD3d11Deferred &d3d11() { return d3d11_; }
    GuestReport           &report() { return report_; }

    const char *run()
    {
        { DeferredMapWork work(d3d11_);
          CHECK(work.invariants_hold());
          work.run(steps_, window);
          CHECK(work.invariants_hold()); }
        CHECK(!d3d11_.live());
        report_.finish();
        return report_.text();
    }

private:
    ScriptedD3d11Deferred d3d11_;
    MockKmt               kmt_;
    av::MockPlatform report_memory_;
    GuestReport           report_{report_memory_};
    ScenarioSteps         steps_;
};

}

TEST(deferred_map_work, every_frame_is_recorded_on_the_two_contexts_in_turn_and_every_step_passes)
{ DeferredRig rig;
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.deferred_device") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.deferred_buffers") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.deferred_contexts") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.deferred_frames") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.deferred_readback") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.deferred_contents 256") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.deferred_commit 0") != nullptr);
  CHECK(std::strstr(report, "avd3d11probe: 7 passed, 0 failed") != nullptr);
  CHECK(rig.d3d11().window() == window);
  CHECK_EQ(rig.d3d11().constant_count(), DeferredMapWork::constant_count);
  CHECK_EQ(rig.d3d11().constant_size(0u), 80u);
  CHECK_EQ(rig.d3d11().constant_size(3u), 4112u);
  CHECK_EQ(rig.d3d11().geometry_bytes(), 1u << 20u);
  CHECK_EQ(rig.d3d11().readback_bytes(), DeferredMapWork::head_bytes);
  CHECK_EQ(rig.d3d11().contexts(), 2u);
  CHECK_EQ(rig.d3d11().finishes_on(0u), frames / 2u);
  CHECK_EQ(rig.d3d11().finishes_on(1u), frames / 2u);
  CHECK_EQ(rig.d3d11().calls(Call::execute), frames);
  CHECK_EQ(rig.d3d11().calls(Call::present), frames);
  CHECK_EQ(rig.d3d11().calls(Call::write_constants), frames * DeferredMapWork::constant_count);
  CHECK_EQ(rig.d3d11().calls(Call::discard_geometry), frames);
  CHECK_EQ(rig.d3d11().calls(Call::append_geometry), frames * (DeferredMapWork::pieces - 1u));
  CHECK_EQ(rig.d3d11().calls(Call::read_geometry), 1u);
  CHECK_EQ(rig.d3d11().calls(Call::release), 1u); }

TEST(deferred_map_work, a_mebibyte_kept_every_measured_frame_fails_the_commit_with_the_growth)
{ DeferredRig rig;
  rig.d3d11().commit_at_finish(first_measured,
                               static_cast<std::int64_t>(DeferredMapWork::measured_frames) << 20u);
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.deferred_contents 256") != nullptr);
  CHECK(std::strstr(report, "FAIL d3d11.deferred_commit status=7 307200") != nullptr);
  CHECK(std::strstr(report, "avd3d11probe: 6 passed, 1 failed") != nullptr); }

TEST(deferred_map_work, growth_up_to_the_limit_passes_and_a_byte_more_fails)
{ { DeferredRig rig;
    rig.d3d11().commit_at_finish(frames, static_cast<std::int64_t>(DeferredMapWork::most_growth));
    const char *report = rig.run();
    CHECK(std::strstr(report, "ok d3d11.deferred_commit 16384") != nullptr); }
  DeferredRig rig;
  rig.d3d11().commit_at_finish(frames, static_cast<std::int64_t>(DeferredMapWork::most_growth) + 1);
  const char *report = rig.run();
  CHECK(std::strstr(report, "FAIL d3d11.deferred_commit status=7 16384") != nullptr); }

TEST(deferred_map_work, warm_frames_are_not_measured_and_memory_given_back_is_no_growth)
{ { DeferredRig rig;
    rig.d3d11().commit_at_finish(DeferredMapWork::warm_frames, std::int64_t{1} << 30u);
    const char *report = rig.run();
    CHECK(std::strstr(report, "ok d3d11.deferred_commit 0") != nullptr); }
  DeferredRig rig;
  rig.d3d11().commit_at_finish(first_measured, -(std::int64_t{64} << 20u));
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.deferred_commit 0") != nullptr); }

TEST(deferred_map_work, appends_that_are_lost_fail_the_contents_with_the_bytes_right)
{ DeferredRig rig;
  rig.d3d11().lose_appends();
  const char *report = rig.run();
  CHECK(std::strstr(report, "FAIL d3d11.deferred_contents status=7 64") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.deferred_commit 0") != nullptr); }

TEST(deferred_map_work, an_execute_that_runs_the_list_before_fails_the_contents)
{ DeferredRig rig;
  rig.d3d11().lag_executes();
  const char *report = rig.run();
  CHECK(std::strstr(report, "FAIL d3d11.deferred_contents status=7 0") != nullptr); }

TEST(deferred_map_work, a_refusal_at_each_call_stops_the_run_there)
{ constexpr std::int32_t e_outofmemory = static_cast<std::int32_t>(0x8007000E);
  struct Refusal {
      Call          call;
      std::uint32_t nth;
      const char   *step;
      std::uint32_t passed;
  };
  const Refusal refusals[] = {
      {Call::create_device, 1u, "FAIL d3d11.deferred_device", 0u},
      {Call::create_buffers, 1u, "FAIL d3d11.deferred_buffers", 1u},
      {Call::create_contexts, 1u, "FAIL d3d11.deferred_contexts", 2u},
      {Call::write_constants, 5u, "FAIL d3d11.deferred_frames", 3u},
      {Call::discard_geometry, 300u, "FAIL d3d11.deferred_frames", 3u},
      {Call::append_geometry, 2u, "FAIL d3d11.deferred_frames", 3u},
      {Call::finish, frames, "FAIL d3d11.deferred_frames", 3u},
      {Call::execute, 1u, "FAIL d3d11.deferred_frames", 3u},
      {Call::present, 31u, "FAIL d3d11.deferred_frames", 3u},
      {Call::read_geometry, 1u, "FAIL d3d11.deferred_readback", 4u},
  };
  for (const Refusal &refusal : refusals) {
      DeferredRig rig;
      rig.d3d11().fail(refusal.call, refusal.nth, e_outofmemory);
      const char *report = rig.run();
      CHECK(std::strstr(report, refusal.step) != nullptr);
      CHECK(std::strstr(report, "2147942414") != nullptr);
      CHECK_EQ(rig.report().passed(), refusal.passed);
      CHECK_EQ(rig.report().failed(), 1u);
      CHECK_EQ(rig.d3d11().calls(Call::release), refusal.call != Call::create_device ? 1u : 0u);
  }
}

TEST(deferred_map_work, a_refused_frame_is_the_last_drawn)
{ DeferredRig rig;
  rig.d3d11().fail(Call::execute, 10u, static_cast<std::int32_t>(0x887A0005));
  static_cast<void>(rig.run());
  CHECK_EQ(rig.d3d11().calls(Call::execute), 10u);
  CHECK_EQ(rig.d3d11().calls(Call::present), 9u);
  CHECK_EQ(rig.d3d11().calls(Call::finish), 10u); }

TEST(deferred_map_work, an_impossible_state_is_named)
{ ScriptedD3d11Deferred d3d11;
  DeferredMapWork       untouched(d3d11);
  CHECK(untouched.invariants_hold());
  DeferredMapWork drew(d3d11);
  DeferredMapWorkProbe::drew_without_a_device(drew);
  CHECK(!drew.invariants_hold());
  DeferredMapWork grew(d3d11);
  DeferredMapWorkProbe::grew_without_a_device(grew);
  CHECK(!grew.invariants_hold());
  CHECK_EQ(DeferredMapWorkProbe::matching(untouched), 0u);
  DeferredMapWork past(d3d11);
  DeferredMapWorkProbe::drew_past_the_frames(past);
  CHECK(!past.invariants_hold()); }
