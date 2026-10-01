/* The D3D9 proof over a scripted D3D9 (feature 047, US1): every step in order,
 * the frame judged against the colour it was cleared to, a refusal at each
 * call kept as the HRESULT it is, and the device released whatever happened. */
#include "guest/suite/d3d9_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d9.hpp"
#include "harness/guest/scripted_image_sink.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct D3d9WorkProbe {
    static std::uint8_t *frame(D3d9Work &work) { return work.frame_; }
    static void          read_without_a_device(D3d9Work &work) { work.read_ = true; }
    static void          adapter_without_a_device(D3d9Work &work) { work.adapter_ = 0x99u; }
    static std::uint32_t coloured(const D3d9Work &work) { return work.coloured(); }
};

}  // namespace av

using av::D3d9Work;
using av::D3d9WorkProbe;
using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;
using av::ScriptedD3d9;
using av::ScriptedImageSink;
using Call = av::ScriptedD3d9::Call;

namespace {

void *const window = reinterpret_cast<void *>(0x9002u);

class D3d9Rig {
public:
    D3d9Rig() : report_("avd3d9probe"), steps_(kmt_, report_, "d3d9") {}
    D3d9Rig(const D3d9Rig &) = delete;
    D3d9Rig &operator=(const D3d9Rig &) = delete;

    ScriptedD3d9      &d3d9() { return d3d9_; }
    ScriptedImageSink &images() { return images_; }
    GuestReport       &report() { return report_; }

    const char *run()
    {
        { D3d9Work work(d3d9_, images_);
          CHECK(work.invariants_hold());
          work.run(steps_, window);
          CHECK(work.invariants_hold());
          adapter_ = work.adapter(); }
        CHECK(!d3d9_.live());
        report_.finish();
        return report_.text();
    }

    std::uint64_t adapter() const { return adapter_; }

private:
    ScriptedD3d9      d3d9_;
    ScriptedImageSink images_;
    MockKmt           kmt_;
    GuestReport       report_;
    ScenarioSteps     steps_;
    std::uint64_t     adapter_ = 0u;
};

}  // namespace

TEST(d3d9_work, every_step_passes_and_the_frame_is_the_colour_it_was_cleared_to)
{ D3d9Rig     rig;
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d9.device") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.adapter_ids") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.clear") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.readback") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.pixels 4096") != nullptr);
  CHECK(std::strstr(report, "ok d3d9.present") != nullptr);
  CHECK(std::strstr(report, "avd3d9probe: 6 passed, 0 failed") != nullptr);
  /* The vendor in the high half, the device in the low. */
  CHECK_EQ(rig.adapter(), (static_cast<std::uint64_t>(ScriptedD3d9::vendor) << 32) | ScriptedD3d9::device);
  CHECK(rig.d3d9().window() == window);
  CHECK_EQ(rig.d3d9().width(), D3d9Work::extent);
  CHECK_EQ(rig.d3d9().calls(Call::release), 1u);
  CHECK(rig.images().size("d3d9-clear") > 0u); }

TEST(d3d9_work, a_frame_of_the_wrong_colour_fails_with_the_count)
{ for (unsigned channel = 0u; channel < 3u; channel++) {
      D3d9Rig rig;
      rig.d3d9().miscolour(channel);
      const char *report = rig.run();
      CHECK(std::strstr(report, "FAIL d3d9.pixels") != nullptr);
      CHECK(std::strstr(report, " 4095\n") != nullptr);
      CHECK(std::strstr(report, "ok d3d9.present") != nullptr);
      CHECK(std::strstr(report, "avd3d9probe: 5 passed, 1 failed") != nullptr);
  }
  /* X8R8G8B8: the fourth byte is not a channel at all. */
  D3d9Rig unused;
  unused.d3d9().miscolour(3u);
  CHECK(std::strstr(unused.run(), "ok d3d9.pixels 4096") != nullptr); }

TEST(d3d9_work, a_refusal_at_each_call_stops_the_run_there)
{ constexpr std::int32_t d3derr_invalidcall = static_cast<std::int32_t>(0x8876086C);
  struct Refusal {
      Call          call;
      const char   *step;
      std::uint32_t passed;
  };
  const Refusal refusals[] = {
      {Call::create_device, "FAIL d3d9.device", 0u},
      {Call::adapter_ids, "FAIL d3d9.adapter_ids", 1u},
      {Call::clear, "FAIL d3d9.clear", 2u},
      {Call::read_back, "FAIL d3d9.readback", 3u},
      {Call::present, "FAIL d3d9.present", 5u},
  };
  for (const Refusal &refusal : refusals) {
      D3d9Rig rig;
      rig.d3d9().fail(refusal.call, 1u, d3derr_invalidcall);
      const char *report = rig.run();
      CHECK(std::strstr(report, refusal.step) != nullptr);
      /* D3DERR_INVALIDCALL as its two's-complement bits. */
      CHECK(std::strstr(report, "2289436780") != nullptr);
      CHECK_EQ(rig.report().passed(), refusal.passed);
      CHECK_EQ(rig.report().failed(), 1u);
      const bool made = refusal.call != Call::create_device;
      CHECK_EQ(rig.d3d9().calls(Call::release), made ? 1u : 0u);
  }
}

TEST(d3d9_work, rounding_is_within_a_step_and_an_impossible_state_is_named)
{ { ScriptedD3d9      d3d9;
    ScriptedImageSink images;
    MockKmt           kmt;
    GuestReport       report("avd3d9probe");
    ScenarioSteps     steps(kmt, report, "d3d9");
    D3d9Work          work(d3d9, images);
    work.run(steps, window);
    std::uint8_t *const frame = D3d9WorkProbe::frame(work);
    for (std::uint32_t pixel = 0u; pixel < D3d9Work::pixels; pixel++)
        for (std::uint32_t channel = 0u; channel < 3u; channel++) {
            std::uint8_t &byte = frame[pixel * D3d9Work::pixel_bytes + channel];
            byte = static_cast<std::uint8_t>(pixel % 2u == 0u ? byte + 1u : byte - 1u);
        }
    CHECK_EQ(D3d9WorkProbe::coloured(work), D3d9Work::pixels);
    frame[2] = static_cast<std::uint8_t>(frame[2] + 2u);
    CHECK_EQ(D3d9WorkProbe::coloured(work), D3d9Work::pixels - 1u); }

  ScriptedD3d9      d3d9;
  ScriptedImageSink images;
  D3d9Work          work(d3d9, images);
  CHECK(work.invariants_hold());
  D3d9WorkProbe::read_without_a_device(work);
  CHECK(!work.invariants_hold());
  /* The other two: something learnt from a device that was never made. */
  D3d9Work adapter(d3d9, images);
  D3d9WorkProbe::adapter_without_a_device(adapter);
  CHECK(!adapter.invariants_hold()); }
