#include "guest/probe/wpf_present_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d9ex.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "harness/guest/scripted_window_frame.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct WpfPresentWorkProbe {
    static void looked_without_a_device(WpfPresentWork &work) { work.looks_ = 1u; }
    static void read_without_a_device(WpfPresentWork &work) { work.read_ = true; }
    static void looked_past_the_blanks(WpfPresentWork &work)
    {
        work.made_ = true;
        work.looks_ = WpfPresentWork::blanks + 1u;
    }
};

}

using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;
using av::ScriptedD3d9Ex;
using av::ScriptedImageSink;
using av::ScriptedWindowFrame;
using av::WpfPresentWork;
using av::WpfPresentWorkProbe;
using Call = av::ScriptedD3d9Ex::Call;

namespace {

void *const window = reinterpret_cast<void *>(0x9003u);

class WpfRig {
public:
    WpfRig() : d3d9_(windows_), report_(report_memory_, "avd3d9probe"), steps_(kmt_, report_, "d3d9")
    {
        windows_.set_window(window, WpfPresentWork::extent, WpfPresentWork::extent);
    }
    WpfRig(const WpfRig &) = delete;
    WpfRig &operator=(const WpfRig &) = delete;

    ScriptedD3d9Ex      &d3d9() { return d3d9_; }
    ScriptedWindowFrame &windows() { return windows_; }
    ScriptedImageSink   &images() { return images_; }
    GuestReport         &report() { return report_; }

    const char *run()
    {
        { WpfPresentWork work(d3d9_, windows_, images_);
          CHECK(work.invariants_hold());
          work.run(steps_, window);
          CHECK(work.invariants_hold()); }
        CHECK(!d3d9_.live());
        report_.finish();
        return report_.text();
    }

private:
    ScriptedWindowFrame windows_;
    ScriptedD3d9Ex      d3d9_;
    ScriptedImageSink   images_;
    MockKmt             kmt_;
    av::MockPlatform report_memory_;
    GuestReport         report_{report_memory_};
    ScenarioSteps       steps_;
};

}

TEST(wpf_present_work, one_present_reaches_the_window_and_every_step_passes)
{ WpfRig      rig;
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d9.wpf_device") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.wpf_swap_chain") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.wpf_draw") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.wpf_present") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.wpf_window 4096") != nullptr);
  CHECK(std::strstr(report, "avd3d9probe: 5 passed, 0 failed") != nullptr);
  CHECK(rig.d3d9().window() == window);
  CHECK_EQ(rig.d3d9().width(), WpfPresentWork::extent);
  CHECK_EQ(rig.d3d9().height(), WpfPresentWork::extent);
  CHECK_EQ(rig.d3d9().calls(Call::present), 1u);
  CHECK_EQ(rig.d3d9().calls(Call::wait_for_blank), 0u);
  CHECK_EQ(rig.d3d9().calls(Call::release), 1u);
  CHECK(rig.images().size("d3d9-wpf") > 0u); }

TEST(wpf_present_work, a_frame_that_arrives_late_is_waited_for)
{ WpfRig rig;
  rig.d3d9().arrive_after(5u);
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d9.wpf_window 4096") != nullptr);
  CHECK_EQ(rig.d3d9().calls(Call::present), 1u);
  CHECK_EQ(rig.d3d9().calls(Call::wait_for_blank), 5u);
  CHECK_EQ(rig.windows().reads(), 6u); }

TEST(wpf_present_work, a_frame_that_never_arrives_fails_the_window_with_the_count)
{ WpfRig rig;
  rig.d3d9().lose_the_frame();
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d9.wpf_present") != nullptr);
  CHECK(std::strstr(report, "FAIL d3d9.wpf_window") != nullptr);
  CHECK(std::strstr(report, "avd3d9probe: 4 passed, 1 failed") != nullptr);
  CHECK_EQ(rig.d3d9().calls(Call::present), 1u);
  CHECK_EQ(rig.d3d9().calls(Call::wait_for_blank), WpfPresentWork::blanks);
  CHECK_EQ(rig.windows().reads(), WpfPresentWork::blanks + 1u);
  CHECK(rig.images().size("d3d9-wpf") > 0u); }

TEST(wpf_present_work, an_unreadable_window_and_a_refused_blank_end_the_watch)
{ { WpfRig rig;
    rig.windows().refuse_read();
    const char *report = rig.run();
    CHECK(std::strstr(report, "FAIL d3d9.wpf_window") != nullptr);
    CHECK_EQ(rig.images().size("d3d9-wpf"), 0u); }
  WpfRig rig;
  rig.d3d9().lose_the_frame();
  rig.d3d9().fail(Call::wait_for_blank, 1u, static_cast<std::int32_t>(0x8876086C));
  const char *report = rig.run();
  CHECK(std::strstr(report, "FAIL d3d9.wpf_window") != nullptr);
  CHECK_EQ(rig.d3d9().calls(Call::wait_for_blank), 1u);
  CHECK_EQ(rig.windows().reads(), 1u); }

TEST(wpf_present_work, a_refusal_at_each_call_stops_the_run_there)
{ constexpr std::int32_t d3derr_invalidcall = static_cast<std::int32_t>(0x8876086C);
  struct Refusal {
      Call          call;
      const char   *step;
      std::uint32_t passed;
  };
  const Refusal refusals[] = {
      {Call::create_device, "FAIL d3d9.wpf_device", 0u},
      {Call::create_swap_chain, "FAIL d3d9.wpf_swap_chain", 1u},
      {Call::draw, "FAIL d3d9.wpf_draw", 2u},
      {Call::present, "FAIL d3d9.wpf_present", 3u},
  };
  for (const Refusal &refusal : refusals) {
      WpfRig rig;
      rig.d3d9().fail(refusal.call, 1u, d3derr_invalidcall);
      const char *report = rig.run();
      CHECK(std::strstr(report, refusal.step) != nullptr);
      CHECK(std::strstr(report, "2289436780") != nullptr);
      CHECK_EQ(rig.report().passed(), refusal.passed);
      CHECK_EQ(rig.report().failed(), 1u);
      CHECK_EQ(rig.windows().reads(), 0u);
      CHECK_EQ(rig.d3d9().calls(Call::release), refusal.call != Call::create_device ? 1u : 0u);
  }
}

TEST(wpf_present_work, an_impossible_state_is_named)
{ ScriptedWindowFrame windows;
  ScriptedD3d9Ex      d3d9(windows);
  ScriptedImageSink   images;
  WpfPresentWork      untouched(d3d9, windows, images);
  CHECK(untouched.invariants_hold());
  WpfPresentWork looked(d3d9, windows, images);
  WpfPresentWorkProbe::looked_without_a_device(looked);
  CHECK(!looked.invariants_hold());
  WpfPresentWork read(d3d9, windows, images);
  WpfPresentWorkProbe::read_without_a_device(read);
  CHECK(!read.invariants_hold());
  WpfPresentWork past(d3d9, windows, images);
  WpfPresentWorkProbe::looked_past_the_blanks(past);
  CHECK(!past.invariants_hold()); }
