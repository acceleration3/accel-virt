/* Render targets shared between processes, the size of a captured window,
 * cleared and read over a scripted Direct3D 11: every step in order, each
 * target's last texel its own colour, and the device still standing - and
 * the failures a host showed: a GPU that loses the device clearing rows no
 * multiple of 128 bytes, as NVIDIA's did under a Windows.Graphics.Capture
 * window capture, and rows read from where the writer did not put them. A
 * refusal at each call is kept as its HRESULT, and the device is released
 * whatever happened. */
#include "guest/probe/shared_clear_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d11_shared.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct SharedClearWorkProbe {
    static void read_without_a_device(SharedClearWork &work) { work.read_ = 1u; }
    static void read_past_what_was_made(SharedClearWork &work)
    {
        work.made_ = true;
        work.made_targets_ = 1u;
        work.read_ = 2u;
    }
};

}  // namespace av

using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;
using av::ScriptedD3d11Shared;
using av::SharedClearWork;
using av::SharedClearWorkProbe;
using Call = av::ScriptedD3d11Shared::Call;

namespace {

class SharedRig {
public:
    SharedRig() : report_("avd3d11probe"), steps_(kmt_, report_, "d3d11") {}
    SharedRig(const SharedRig &) = delete;
    SharedRig &operator=(const SharedRig &) = delete;

    ScriptedD3d11Shared &d3d11() { return d3d11_; }
    GuestReport         &report() { return report_; }

    const char *run()
    {
        { SharedClearWork work(d3d11_);
          CHECK(work.invariants_hold());
          work.run(steps_);
          CHECK(work.invariants_hold()); }
        CHECK(!d3d11_.live());
        report_.finish();
        return report_.text();
    }

private:
    ScriptedD3d11Shared d3d11_;
    MockKmt             kmt_;
    GuestReport         report_;
    ScenarioSteps       steps_;
};

}  // namespace

TEST(shared_clear_work, every_target_is_cleared_to_its_colour_and_read_at_its_last_texel)
{ SharedRig rig;
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.shared_device") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.shared_targets") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.shared_readback") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.shared_texels 3") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.shared_device_kept") != nullptr);
  CHECK(std::strstr(report, "avd3d11probe: 5 passed, 0 failed") != nullptr);
  /* The capture that lost the device, 2456 bytes a row. */
  CHECK_EQ(rig.d3d11().width(0u), 614u);
  CHECK_EQ(rig.d3d11().height(0u), 289u);
  CHECK_EQ(rig.d3d11().calls(Call::create_target), SharedClearWork::targets);
  CHECK_EQ(rig.d3d11().calls(Call::clear), SharedClearWork::targets);
  CHECK_EQ(rig.d3d11().calls(Call::read_texel), SharedClearWork::targets);
  /* The last read was the single texel's only one. */
  CHECK_EQ(rig.d3d11().last_x(), 0u);
  CHECK_EQ(rig.d3d11().last_y(), 0u);
  CHECK_EQ(rig.d3d11().calls(Call::release), 1u);
  /* No target's tight rows are a multiple of 128 bytes: each is a pitch
   * NVIDIA's driver chose and its 3D engine could not clear. */
  for (const std::uint32_t width : SharedClearWork::widths)
      CHECK((width * SharedClearWork::texel_bytes) % 128u != 0u); }

/* What NVIDIA's GPU did: the first clear of a target whose rows are not
 * 128-byte aligned lost the device, so nothing reads back. */
TEST(shared_clear_work, a_device_lost_by_a_clear_fails_the_readback_with_its_hresult)
{ SharedRig rig;
  rig.d3d11().lose_device_below(128u);
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.shared_targets") != nullptr);
  CHECK(std::strstr(report, "FAIL d3d11.shared_readback") != nullptr);
  CHECK(std::strstr(report, "2289696773") != nullptr);   /* DXGI_ERROR_DEVICE_REMOVED */
  CHECK(std::strstr(report, "shared_texels") == nullptr);
  CHECK(std::strstr(report, "avd3d11probe: 2 passed, 1 failed") != nullptr);
  CHECK_EQ(rig.d3d11().calls(Call::read_texel), 1u); }

/* A host that rounds the rows up to 256 bytes clears every target. */
TEST(shared_clear_work, rows_a_pitch_the_gpu_draws_at_lose_nothing)
{ SharedRig rig;
  rig.d3d11().lose_device_below(4u);
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.shared_device_kept") != nullptr);
  CHECK(std::strstr(report, "avd3d11probe: 5 passed, 0 failed") != nullptr); }

/* Rows read from other than where they were written miss the last texel of
 * every target taller than a row: the single texel is still right. */
TEST(shared_clear_work, rows_out_of_place_fail_the_texels_with_the_targets_right)
{ SharedRig rig;
  rig.d3d11().misplace_rows();
  const char *report = rig.run();
  CHECK(std::strstr(report, "FAIL d3d11.shared_texels status=7 1") != nullptr);
  CHECK(std::strstr(report, "ok d3d11.shared_device_kept") != nullptr); }

/* A device that went away without a read noticing is still a failure. */
TEST(shared_clear_work, a_device_removed_after_the_reads_fails_the_last_step)
{ SharedRig rig;
  rig.d3d11().fail(Call::removed_reason, 1u, ScriptedD3d11Shared::device_hung);
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d11.shared_texels 3") != nullptr);
  CHECK(std::strstr(report, "FAIL d3d11.shared_device_kept") != nullptr);
  CHECK(std::strstr(report, "2289696774") != nullptr); }

TEST(shared_clear_work, a_refusal_at_each_call_stops_the_run_there)
{ constexpr std::int32_t e_outofmemory = static_cast<std::int32_t>(0x8007000E);
  struct Refusal {
      Call          call;
      std::uint32_t nth;
      const char   *step;
      std::uint32_t passed;
  };
  const Refusal refusals[] = {
      {Call::create_device, 1u, "FAIL d3d11.shared_device", 0u},
      {Call::create_target, 1u, "FAIL d3d11.shared_targets", 1u},
      {Call::create_target, 3u, "FAIL d3d11.shared_targets", 1u},
      {Call::read_texel, 2u, "FAIL d3d11.shared_readback", 2u},
  };
  for (const Refusal &refusal : refusals) {
      SharedRig rig;
      rig.d3d11().fail(refusal.call, refusal.nth, e_outofmemory);
      const char *report = rig.run();
      CHECK(std::strstr(report, refusal.step) != nullptr);
      CHECK(std::strstr(report, "2147942414") != nullptr);
      CHECK_EQ(rig.report().passed(), refusal.passed);
      CHECK_EQ(rig.report().failed(), 1u);
      CHECK_EQ(rig.d3d11().calls(Call::release), refusal.call != Call::create_device ? 1u : 0u);
  }
}

TEST(shared_clear_work, an_impossible_state_is_named)
{ ScriptedD3d11Shared d3d11;
  SharedClearWork     untouched(d3d11);
  CHECK(untouched.invariants_hold());
  SharedClearWork read(d3d11);
  SharedClearWorkProbe::read_without_a_device(read);
  CHECK(!read.invariants_hold());
  SharedClearWork past(d3d11);
  SharedClearWorkProbe::read_past_what_was_made(past);
  CHECK(!past.invariants_hold()); }
