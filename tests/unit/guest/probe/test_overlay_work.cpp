/* The overlay proof over a scripted DXGI - an OverlayEntry whose
 * supports_overlays, overlay support and composition modes the test sets:
 * every step in order, a swap chain on part of the screen and one on all of it
 * each judged by whether DWM showed it in an overlay plane, a refusal at each call kept as the HRESULT it is, a
 * composed swap chain failed with its mode on the line, and the device released
 * whatever happened. */
#include "guest/probe/overlay_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_overlay.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::GuestReport;
using av::MockKmt;
using av::OverlayWork;
using av::ScenarioSteps;
using av::ScriptedOverlay;
using Call = av::ScriptedOverlay::Call;

namespace {

class OverlayRig {
public:
    explicit OverlayRig(std::uint32_t hold = 0u) : report_("avoverlayprobe"), steps_(kmt_, report_, "overlay"), hold_(hold) {}
    OverlayRig(const OverlayRig &) = delete;
    OverlayRig &operator=(const OverlayRig &) = delete;

    ScriptedOverlay &dxgi() { return dxgi_; }

    const char *run()
    {
        { OverlayWork work(dxgi_, hold_);
          CHECK(work.invariants_hold());
          work.run(steps_);
          CHECK(work.invariants_hold());
          mode_ = work.mode(); }
        report_.finish();
        return report_.text();
    }
    std::uint32_t failed() const { return report_.failed(); }
    std::uint32_t mode() const { return mode_; }

private:
    MockKmt         kmt_;
    GuestReport     report_;
    ScenarioSteps   steps_;
    ScriptedOverlay dxgi_;
    std::uint32_t   hold_;
    std::uint32_t   mode_ = 0u;
};

bool says(const char *report, const char *line) { return std::strstr(report, line) != nullptr; }

}  // namespace

TEST(overlay_work, proves_both_swap_chains_shown_in_overlay_planes)
{ OverlayRig rig;
  const char *const report = rig.run();
  CHECK_EQ(rig.failed(), 0u);
  CHECK(says(report, "overlays"));
  CHECK(says(report, "video_overlay_support"));
  CHECK_EQ(rig.dxgi().asked_format(0u), OverlayWork::format_bgra);
  CHECK_EQ(rig.dxgi().asked_format(1u), OverlayWork::format_nv12);
  CHECK(says(report, "overlay_plane"));
  CHECK(says(report, "direct_flip"));
  CHECK_EQ(rig.dxgi().calls(Call::create_swap_chain), 2u);
  CHECK_EQ(rig.dxgi().calls(Call::present), 2u * OverlayWork::presents);
  CHECK_EQ(rig.dxgi().calls(Call::composition), 2u);
  CHECK_EQ(rig.dxgi().x(), 0);          /* the last covers the output */
  CHECK_EQ(rig.dxgi().width(), 0u);
  CHECK_EQ(rig.mode(), OverlayWork::mode_overlay);
  CHECK(!rig.dxgi().live()); }

TEST(overlay_work, fails_a_composed_swap_chain_and_goes_on)
{ OverlayRig rig;
  rig.dxgi().answer(true, 1u, 0u);   /* composed */
  rig.run();
  CHECK_EQ(rig.failed(), 2u);
  CHECK_EQ(rig.dxgi().calls(Call::create_swap_chain), 2u);
  CHECK_EQ(rig.mode(), 0u);
  CHECK(!rig.dxgi().live()); }

/* An output without video overlays may still put an RGB swap chain in a
 * plane, so the swap chains are still tried. */
TEST(overlay_work, goes_on_when_the_output_has_no_video_overlays)
{ OverlayRig rig;
  rig.dxgi().answer(false, 1u, 1u);
  rig.run();
  CHECK_EQ(rig.failed(), 1u);
  CHECK_EQ(rig.dxgi().calls(Call::overlay_support), 2u);
  CHECK_EQ(rig.dxgi().calls(Call::create_swap_chain), 2u);
  CHECK(!rig.dxgi().live()); }

TEST(overlay_work, keeps_each_refusal_and_releases_what_it_made)
{ for (unsigned call = 0u; call < static_cast<unsigned>(Call::release); call++) {
      OverlayRig rig;
      rig.dxgi().fail(static_cast<Call>(call), 1u, static_cast<std::int32_t>(0x887A0001u));
      const char *const report = rig.run();
      CHECK(rig.failed() >= 1u);
      CHECK(says(report, "2289696769"));   /* 0x887A0001, as the report writes it */
      CHECK(!rig.dxgi().live());
  } }

TEST(overlay_work, a_failed_present_is_one_line_and_ends_that_swap_chain)
{ OverlayRig rig;
  rig.dxgi().fail(Call::present, 3u, static_cast<std::int32_t>(0x887A0005u));
  rig.run();
  CHECK_EQ(rig.dxgi().calls(Call::composition), 0u);
  CHECK_EQ(rig.dxgi().calls(Call::present), 4u);   /* the third, then the second chain's first */
  CHECK(!rig.dxgi().live()); }

/* Statistics that restarted are asked for again after another present, and a
 * swap chain that keeps restarting them fails with the answer it gave. */
TEST(overlay_work, asks_again_while_the_statistics_are_disjoint)
{ OverlayRig rig;
  rig.dxgi().fail(Call::composition, 1u, OverlayWork::disjoint);
  rig.run();
  CHECK_EQ(rig.dxgi().calls(Call::composition), 2u * (1u + OverlayWork::disjoint_retries));
  CHECK_EQ(rig.dxgi().calls(Call::present), 2u * (OverlayWork::presents + OverlayWork::disjoint_retries));
  CHECK_EQ(rig.failed(), 2u); }

/* Either kind of overlay support is support; none is a failure. */
TEST(overlay_work, takes_direct_or_scaling_overlay_support)
{ for (const std::uint32_t flags : {OverlayWork::support_direct, OverlayWork::support_scaling, 0u}) {
      OverlayRig rig;
      rig.dxgi().answer(true, flags, OverlayWork::mode_overlay);
      rig.run();
      CHECK_EQ(rig.failed(), flags == 0u ? 2u : 0u);   /* the desktop's and video's */
  } }

/* A hold keeps the partial window presenting after it is judged, for a check
 * from outside the guest, and the full-screen one after it is not held. */
TEST(overlay_work, holds_the_partial_window_when_asked)
{ OverlayRig rig(40u);
  rig.run();
  CHECK_EQ(rig.failed(), 0u);
  CHECK_EQ(rig.dxgi().calls(Call::present), 2u * OverlayWork::presents + 40u); }
