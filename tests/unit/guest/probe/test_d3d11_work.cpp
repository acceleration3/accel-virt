#include "guest/probe/d3d11_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d11.hpp"
#include "harness/guest/scripted_image_sink.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct D3d11WorkProbe {
    static std::uint8_t *frame(D3d11Work &work) { return work.frame_; }
    static void          read_without_a_device(D3d11Work &work) { work.read_ = true; }
    static void          adapter_without_a_device(D3d11Work &work) { work.adapter_ = 0x99u; }
    static void          level_without_a_device(D3d11Work &work) { work.feature_level_ = 0xB000u; }
    static std::uint32_t coloured(const D3d11Work &work) { return work.coloured(); }
};

}

using av::D3d11Work;
using av::D3d11WorkProbe;
using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;
using av::ScriptedD3d11;
using av::ScriptedImageSink;
using Call = av::ScriptedD3d11::Call;

namespace {

void *const window = reinterpret_cast<void *>(0x9001u);

class D3d11Rig {
public:
    D3d11Rig() : report_(report_memory_, "avd3d11probe"), steps_(kmt_, report_, "d3d11") {}
    D3d11Rig(const D3d11Rig &) = delete;
    D3d11Rig &operator=(const D3d11Rig &) = delete;

    ScriptedD3d11     &d3d11() { return d3d11_; }
    ScriptedImageSink &images() { return images_; }
    GuestReport       &report() { return report_; }

    const char *run()
    {
        { D3d11Work work(d3d11_, images_);
          CHECK(work.invariants_hold());
          work.run(steps_, window);
          CHECK(work.invariants_hold());
          adapter_ = work.adapter();
          feature_level_ = work.feature_level(); }
        CHECK(!d3d11_.live());
        report_.finish();
        return report_.text();
    }

    std::uint64_t adapter() const { return adapter_; }
    std::uint32_t feature_level() const { return feature_level_; }

private:
    ScriptedD3d11     d3d11_;
    ScriptedImageSink images_;
    MockKmt           kmt_;
    av::MockPlatform report_memory_;
    GuestReport       report_{report_memory_};
    ScenarioSteps     steps_;
    std::uint64_t     adapter_ = 0u;
    std::uint32_t     feature_level_ = 0u;
};

}

TEST(d3d11_work, every_step_passes_and_the_frame_is_the_colour_it_was_cleared_to)
{ D3d11Rig    rig;
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.device") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.feature_level 45056") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.adapter_luid") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.target") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.readback") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.pixels 4096") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.present") != nullptr);
  CHECK(std::strstr(report, "avd3d11probe: 7 passed, 0 failed") != nullptr);
  CHECK_EQ(rig.adapter(), ScriptedD3d11::adapter);
  CHECK_EQ(rig.feature_level(), ScriptedD3d11::feature_level_11_0);
  CHECK(rig.d3d11().window() == window);
  CHECK_EQ(rig.d3d11().width(), D3d11Work::extent);
  CHECK_EQ(rig.d3d11().calls(Call::clear), 1u);
  CHECK_EQ(rig.d3d11().calls(Call::release), 1u);
  CHECK(rig.images().size("d3d11-clear") > 0u); }

TEST(d3d11_work, a_frame_of_the_wrong_colour_fails_with_the_count)
{
  for (unsigned channel = 0u; channel < 3u; channel++) {
      D3d11Rig rig;
      rig.d3d11().miscolour(channel);
      const char *report = rig.run();
      CHECK(std::strstr(report, "FAIL d3d11.pixels") != nullptr);
      CHECK(std::strstr(report, " 4095\n") != nullptr);
      CHECK(std::strstr(report, "ok d3d11.present") != nullptr);
      CHECK(std::strstr(report, "avd3d11probe: 6 passed, 1 failed") != nullptr);
  }
  D3d11Rig alpha;
  alpha.d3d11().miscolour(3u);
  CHECK(std::strstr(alpha.run(), "ok d3d11.pixels 4096") != nullptr); }

TEST(d3d11_work, a_refusal_at_each_call_stops_the_run_there)
{ constexpr std::int32_t e_fail = static_cast<std::int32_t>(0x80004005);
  struct Refusal {
      Call        call;
      const char *step;
      std::uint32_t passed;
  };
  const Refusal refusals[] = {
      {Call::create_device, "FAIL d3d11.device", 0u},
      {Call::adapter_luid, "FAIL d3d11.adapter_luid", 2u},
      {Call::create_render_target, "FAIL d3d11.target", 3u},
      {Call::read_back, "FAIL d3d11.readback", 4u},
      {Call::present, "FAIL d3d11.present", 6u},
  };
  for (const Refusal &refusal : refusals) {
      D3d11Rig rig;
      rig.d3d11().fail(refusal.call, 1u, e_fail);
      const char *report = rig.run();
      CHECK(std::strstr(report, refusal.step) != nullptr);
      CHECK(std::strstr(report, "2147500037") != nullptr);
      CHECK_EQ(rig.report().passed(), refusal.passed);
      CHECK_EQ(rig.report().failed(), 1u);
      const bool made = refusal.call != Call::create_device;
      CHECK_EQ(rig.d3d11().calls(Call::release), made ? 1u : 0u);
  }
}

TEST(d3d11_work, rounding_is_within_a_step_and_an_impossible_state_is_named)
{ { D3d11Rig          rig;
    ScriptedD3d11    &d3d11 = rig.d3d11();
    ScriptedImageSink images;
    MockKmt           kmt;
    av::MockPlatform report_memory;
    GuestReport       report(report_memory, "avd3d11probe");
    ScenarioSteps     steps(kmt, report, "d3d11");
    D3d11Work         work(d3d11, images);
    work.run(steps, window);
    std::uint8_t *const frame = D3d11WorkProbe::frame(work);
    for (std::uint32_t pixel = 0u; pixel < D3d11Work::pixels; pixel++)
        for (std::uint32_t channel = 0u; channel < 3u; channel++) {
            std::uint8_t &byte = frame[pixel * D3d11Work::pixel_bytes + channel];
            byte = static_cast<std::uint8_t>(pixel % 2u == 0u ? byte + 1u : byte - 1u);
        }
    CHECK(work.invariants_hold());
    CHECK_EQ(D3d11WorkProbe::coloured(work), D3d11Work::pixels);
    frame[0] = static_cast<std::uint8_t>(frame[0] + 2u);
    CHECK_EQ(D3d11WorkProbe::coloured(work), D3d11Work::pixels - 1u); }

  ScriptedD3d11     d3d11;
  ScriptedImageSink images;
  D3d11Work         work(d3d11, images);
  CHECK(work.invariants_hold());
  D3d11WorkProbe::read_without_a_device(work);
  CHECK(!work.invariants_hold());
  D3d11Work adapter(d3d11, images);
  D3d11WorkProbe::adapter_without_a_device(adapter);
  CHECK(!adapter.invariants_hold());
  D3d11Work level(d3d11, images);
  D3d11WorkProbe::level_without_a_device(level);
  CHECK(!level.invariants_hold()); }
