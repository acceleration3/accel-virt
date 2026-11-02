#include "guest/probe/tir_coverage_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d11_tir.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct TirCoverageWorkProbe {
    static void read_without_a_device(TirCoverageWork &work) { work.read_ = true; }
    static void lose_a_vertex(TirCoverageWork &work) { work.added_--; }
    static std::uint32_t partial(const TirCoverageWork &work)
    {
        std::uint32_t count = 0u;
        for (const std::uint32_t mask : work.expected_) count += mask != 0u && mask != 0xffffu ? 1u : 0u;
        return count;
    }
};

}

using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;
using av::ScriptedD3d11Tir;
using av::TirCoverageWork;
using av::TirCoverageWorkProbe;
using Call = av::ScriptedD3d11Tir::Call;

namespace {

constexpr std::int32_t invalid_argument = static_cast<std::int32_t>(0x80070057u);

class TirRig {
public:
    TirRig() : report_(report_memory_, "avd3d11probe"), steps_(kmt_, report_, "d3d11") {}
    TirRig(const TirRig &) = delete;
    TirRig &operator=(const TirRig &) = delete;

    ScriptedD3d11Tir &d3d11() { return d3d11_; }

    const char *run()
    {
        { TirCoverageWork work(d3d11_);
          CHECK(work.invariants_hold());
          work.run(steps_);
          CHECK(work.invariants_hold()); }
        CHECK(!d3d11_.live());
        report_.finish();
        return report_.text();
    }

private:
    ScriptedD3d11Tir d3d11_;
    MockKmt          kmt_;
    av::MockPlatform report_memory_;
    GuestReport      report_{report_memory_};
    ScenarioSteps    steps_;
};

bool has(const char *report, const char *line)
{
    return std::strstr(report, line) != nullptr;
}

}

TEST(tir_coverage_work, hairlines_at_a_forced_16_samples_come_back_as_d3d_covers_them)
{ TirRig rig;
  const char *report = rig.run();
  CHECK(has(report, "ok d3d11.tir_device"));
  CHECK(has(report, "ok d3d11.tir_pipeline"));
  CHECK(has(report, "ok d3d11.tir_draw"));
  CHECK(has(report, "ok d3d11.tir_readback"));
  CHECK(has(report, "ok d3d11.tir_coverage 4096"));
  CHECK(has(report, "ok d3d11.tir_device_kept"));
  CHECK(has(report, "avd3d11probe: 6 passed, 0 failed"));
  CHECK_EQ(rig.d3d11().extent(), TirCoverageWork::extent);
  CHECK_EQ(rig.d3d11().samples(), 16u);
  CHECK_EQ(rig.d3d11().vertices(), TirCoverageWork::vertices);
  CHECK_EQ(rig.d3d11().calls(Call::draw), 1u);
  CHECK_EQ(rig.d3d11().calls(Call::release), 1u); }

TEST(tir_coverage_work, the_strokes_cover_texels_partly)
{ ScriptedD3d11Tir d3d11;
  const TirCoverageWork work(d3d11);
  CHECK(TirCoverageWorkProbe::partial(work) > 400u); }

TEST(tir_coverage_work, a_draw_rasterized_at_one_sample_fails_the_coverage_with_what_was_right)
{ TirRig rig;
  rig.d3d11().rasterize_at_one_sample();
  const char *report = rig.run();
  CHECK(has(report, "FAIL d3d11.tir_coverage"));
  CHECK(!has(report, "tir_coverage 4096"));
  CHECK(has(report, "ok d3d11.tir_device_kept"));
  CHECK(has(report, "avd3d11probe: 5 passed, 1 failed")); }

TEST(tir_coverage_work, a_layers_device_is_noted_not_judged)
{ TirRig rig;
  rig.d3d11().set_layered(true);
  rig.d3d11().rasterize_at_one_sample();
  const char *report = rig.run();
  CHECK(has(report, "note d3d11.tir_coverage"));
  CHECK(has(report, "ok d3d11.tir_device_kept"));
  CHECK(has(report, "avd3d11probe: 5 passed, 0 failed")); }

TEST(tir_coverage_work, a_refused_device_fails_the_first_step_and_releases_nothing)
{ TirRig rig;
  rig.d3d11().fail(Call::create_device, 1u, invalid_argument);
  const char *report = rig.run();
  CHECK(has(report, "FAIL d3d11.tir_device"));
  CHECK(has(report, "2147942487"));
  CHECK(!has(report, "tir_pipeline"));
  CHECK_EQ(rig.d3d11().calls(Call::release), 0u); }

TEST(tir_coverage_work, a_refusal_at_each_later_call_fails_its_step_and_releases_the_device)
{ const Call calls[] = {Call::create_pipeline, Call::draw, Call::read_back};
  const char *steps[] = {"FAIL d3d11.tir_pipeline", "FAIL d3d11.tir_draw", "FAIL d3d11.tir_readback"};
  for (std::uint32_t i = 0u; i < 3u; i++) {
      TirRig rig;
      rig.d3d11().fail(calls[i], 1u, invalid_argument);
      const char *report = rig.run();
      CHECK(has(report, steps[i]));
      CHECK(!has(report, "tir_coverage"));
      CHECK(has(report, "1 failed"));
      CHECK_EQ(rig.d3d11().calls(Call::release), 1u);
  } }

TEST(tir_coverage_work, a_device_lost_by_the_draw_fails_the_last_step)
{ TirRig rig;
  rig.d3d11().lose();
  const char *report = rig.run();
  CHECK(has(report, "ok d3d11.tir_coverage 4096"));
  CHECK(has(report, "FAIL d3d11.tir_device_kept"));
  CHECK(has(report, "2289696774")); }

TEST(tir_coverage_work, the_invariants_catch_a_read_without_a_device_and_a_scene_short_of_vertices)
{ ScriptedD3d11Tir d3d11;
  { TirCoverageWork work(d3d11);
    TirCoverageWorkProbe::read_without_a_device(work);
    CHECK(!work.invariants_hold()); }
  { TirCoverageWork work(d3d11);
    TirCoverageWorkProbe::lose_a_vertex(work);
    CHECK(!work.invariants_hold()); } }
