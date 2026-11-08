#include "core/log/driver_log.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/common/present_pacing.hpp"

#include <cstdint>

using av::MockKmt;
using av::icd::PresentPacing;

namespace av::icd {
struct FramePacerProbe {
    static void count_a_blank_without_an_adapter(FramePacer &pacer)
    {
        pacer.adapter_ = 0u;
        pacer.blanks_ = 1u;
    }
};

struct PresentPacingProbe {
    static FramePacer &pacer(PresentPacing &pacing) { return pacing.pacer_.object(); }
    static void        set_blanks(PresentPacing &pacing, std::uint32_t blanks) { pacing.blanks_ = blanks; }
};

}

using av::icd::FramePacerProbe;
using av::icd::PresentPacingProbe;

namespace {

class PacingRig {
public:
    PacingRig()
    { kmt_.add_adapter(av::PciIdentity(av::PciIdentity::virtio_vendor, av::AvgpuAdapters::virtio_gpu_device,
                                       av::PciIdentity::virtio_vendor, av::AvgpuAdapters::av_subsystem),
                       av::AdapterScenario::wddm_3_2);
      std::uint32_t adapters[4] = {};
      std::uint32_t found = 0u;
      CHECK_STATUS(kmt_.enumerate_adapters(adapters, 4u, found), av::Status::ok);
      adapter_ = adapters[0];
      CHECK_STATUS(kmt_.create_device(adapter_, device_), av::Status::ok); }

    av::Status bind(PresentPacing &pacing) { return pacing.bind(kmt_, lines_, adapter_, device_); }
    MockKmt   &kmt() { return kmt_; }

private:
    MockKmt           kmt_;
    av::RecordingLog  log_;
    av::DriverLog     lines_{log_, "avgl: "};
    std::uint32_t     adapter_ = 0u;
    std::uint32_t     device_ = 0u;
};

}

TEST(present_pacing, a_new_pacing_waits_for_one_blank)
{ PresentPacing pacing;
  CHECK(pacing.invariants_hold());
  CHECK_EQ(pacing.blanks(), 1u);
  CHECK_EQ(pacing.waited(), 0u); }

TEST(present_pacing, what_is_asked_for_within_range_is_kept)
{ PresentPacing pacing;
  for (std::int32_t asked = 0; asked <= static_cast<std::int32_t>(PresentPacing::max_blanks); asked++) {
      CHECK(pacing.ask(asked));
      CHECK_EQ(pacing.blanks(), static_cast<std::uint32_t>(asked));
      CHECK(pacing.invariants_hold());
  } }

TEST(present_pacing, an_interval_nobody_defines_is_refused_and_changes_nothing)
{ PresentPacing pacing;
  CHECK(pacing.ask(2));
  const std::int32_t refused[] = {-1, -100, static_cast<std::int32_t>(PresentPacing::max_blanks) + 1, 1000,
                                  -2147483647 - 1};
  for (const std::int32_t asked : refused) {
      CHECK(!pacing.ask(asked));
      CHECK_EQ(pacing.blanks(), 2u);
      CHECK(pacing.invariants_hold());
  } }

TEST(present_pacing, nothing_waits_or_drops_before_the_pacer_is_bound)
{ PresentPacing pacing;
  CHECK(pacing.admits());
  pacing.after_present();
  CHECK_EQ(pacing.waited(), 0u);
  CHECK(pacing.ask(0));
  CHECK(pacing.admits());
  CHECK(pacing.invariants_hold()); }

TEST(present_pacing, every_blank_waited_for_after_a_present_is_counted)
{ PacingRig rig;
  PresentPacing pacing;
  CHECK_STATUS(rig.bind(pacing), av::Status::ok);
  CHECK(pacing.invariants_hold());
  pacing.after_present();
  CHECK_EQ(pacing.waited(), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 1u);
  CHECK(pacing.ask(3));
  pacing.after_present();
  CHECK_EQ(pacing.waited(), 4u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 4u);
  CHECK(pacing.ask(0));
  pacing.after_present();
  CHECK_EQ(pacing.waited(), 4u);
  CHECK(pacing.invariants_hold()); }

TEST(present_pacing, unbinding_stops_the_waiting)
{ PacingRig rig;
  PresentPacing pacing;
  CHECK_STATUS(rig.bind(pacing), av::Status::ok);
  pacing.unbind();
  pacing.after_present();
  CHECK_EQ(pacing.waited(), 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 0u);
  CHECK(pacing.invariants_hold()); }

TEST(present_pacing, its_invariants_catch_too_many_blanks_and_a_pacer_that_is_unsound)
{ PacingRig rig;
  PresentPacing pacing;
  CHECK_STATUS(rig.bind(pacing), av::Status::ok);
  CHECK(pacing.invariants_hold());
  PresentPacingProbe::set_blanks(pacing, PresentPacing::max_blanks + 1u);
  CHECK(!pacing.invariants_hold());
  PresentPacingProbe::set_blanks(pacing, 1u);
  CHECK(pacing.invariants_hold());
  FramePacerProbe::count_a_blank_without_an_adapter(PresentPacingProbe::pacer(pacing));
  CHECK(!pacing.invariants_hold()); }

TEST(present_pacing, the_next_admit_is_a_millisecond_away_until_a_pacer_knows_the_refresh)
{ PacingRig rig;
  PresentPacing pacing;
  CHECK_EQ(pacing.next_admit_milliseconds(), 1u);
  CHECK_STATUS(rig.bind(pacing), av::Status::ok);
  CHECK_EQ(pacing.next_admit_milliseconds(), 17u); }
