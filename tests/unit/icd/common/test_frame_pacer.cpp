/* FIFO's pacing (feature 046, research.md §4): a presented frame is held until
 * the adapter's next vertical blank, and an adapter that cannot be waited on
 * leaves the frame unpaced - said once, however many frames follow. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "icd/common/frame_pacer.hpp"

#include <cstdint>

using av::AvgpuAdapters;
using av::MockKmt;
using av::RecordingLog;
using av::icd::FramePacer;

namespace {

const char *const unpaced = "avvk: FIFO present unpaced";

av::PciIdentity av_identity()
{ return av::PciIdentity(av::AvgpuAdapters::virtio_vendor, av::AvgpuAdapters::virtio_gpu_device,
                         av::AvgpuAdapters::virtio_vendor, av::AvgpuAdapters::av_subsystem); }

/* An adapter and a D3DKMT device on it, as the ICD's connection holds. */
class PacerRig {
public:
    PacerRig()
    { kmt_.add_adapter(av_identity(), av::AdapterScenario::wddm_3_2);
      std::uint32_t adapters[4] = {};
      std::uint32_t found = 0u;
      CHECK_STATUS(kmt_.enumerate_adapters(adapters, 4u, found), av::Status::ok);
      CHECK_EQ(found, 1u);
      adapter_ = adapters[0];
      CHECK_STATUS(kmt_.create_device(adapter_, device_), av::Status::ok); }

    MockKmt      &kmt() { return kmt_; }
    RecordingLog &log() { return log_; }
    std::uint32_t adapter() const { return adapter_; }
    std::uint32_t device() const { return device_; }

private:
    MockKmt       kmt_;
    RecordingLog  log_;
    std::uint32_t adapter_ = 0u;
    std::uint32_t device_ = 0u;
};

}  // namespace

TEST(frame_pacer, a_wait_counts_one_blank_on_the_adapter_s_primary_source)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.log(), rig.adapter(), rig.device());
  CHECK(pacer.invariants_hold());
  CHECK_EQ(pacer.blanks(), 0u);
  pacer.wait();
  CHECK(pacer.invariants_hold());
  CHECK_EQ(pacer.blanks(), 1u);
  CHECK_EQ(pacer.failures(), 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 1u);
  pacer.wait();
  pacer.wait();
  CHECK_EQ(pacer.blanks(), 3u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 3u);
  CHECK_EQ(rig.log().holding(unpaced), 0u);
  CHECK_EQ(rig.kmt().misuses(), 0u); }

/* The frame is already on its window: a failed wait costs pacing, not the
 * present, and a log line for every frame would be the whole log. */
TEST(frame_pacer, a_refused_wait_is_said_once_and_paces_nothing)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.log(), rig.adapter(), rig.device());
  rig.kmt().fail_at(MockKmt::Call::wait_for_vertical_blank, 1u, 0xC0000001u);
  pacer.wait();
  CHECK(pacer.invariants_hold());
  CHECK_EQ(pacer.blanks(), 0u);
  CHECK_EQ(pacer.failures(), 1u);
  CHECK_EQ(rig.log().holding(unpaced), 1u);
  rig.kmt().fail_at(MockKmt::Call::wait_for_vertical_blank, 1u, 0xC0000001u);
  pacer.wait();
  CHECK_EQ(pacer.failures(), 2u);
  CHECK_EQ(rig.log().holding(unpaced), 1u);
  /* A wait that works after one that did not still paces. */
  pacer.wait();
  CHECK_EQ(pacer.blanks(), 1u);
  CHECK_EQ(pacer.failures(), 2u); }

/* A swapchain on a device the ICD did not open through D3DKMT: there is no
 * adapter to wait on, so nothing is waited on. */
TEST(frame_pacer, no_adapter_means_no_wait_at_all)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.log(), 0u, rig.device());
  pacer.wait();
  CHECK(pacer.invariants_hold());
  CHECK_EQ(pacer.blanks(), 0u);
  CHECK_EQ(pacer.failures(), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 0u);
  CHECK_EQ(rig.log().holding(unpaced), 1u); }

namespace {

/* The mock's refresh: a blank every sixtieth of its second, the first at 0. */
constexpr std::uint64_t refresh = MockKmt::ticks_per_second / 60u;
/* The first present, 0.6 into the third refresh: a clock that has run a while,
 * so that a schedule of the pacer's own starts where that present was. */
constexpr std::uint64_t first_present = 2u * refresh + 100000u;

/* Whether each of `count` unpaced presents was admitted: the first at
 * first_present, the rest `step` ticks apart. */
template <std::size_t count>
void admit_every(FramePacer &pacer, MockKmt &kmt, std::uint64_t step, bool (&admitted)[count])
{ kmt.reads_clock(nullptr, first_present);
  admitted[0] = pacer.admits();
  kmt.reads_clock(nullptr, step);
  for (std::size_t i = 1u; i < count; i++) admitted[i] = pacer.admits(); }

}  // namespace

/* Presents 0.3 of a refresh apart: blanks fall before the 3rd, the 6th and,
 * by two ticks, the 9th, and the first frame after each is the one shown. A
 * schedule a refresh apart from the first would have shown the 5th and 8th,
 * late in their refreshes. */
TEST(frame_pacer, the_first_frame_presented_after_each_blank_is_admitted)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.log(), rig.adapter(), rig.device());
  bool admitted[9] = {};
  admit_every(pacer, rig.kmt(), 50000u, admitted);
  const bool expected[9] = {true, false, true, false, false, true, false, false, true};
  for (std::size_t i = 0u; i < 9u; i++) CHECK_EQ(admitted[i], expected[i]);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::scan_line), 9u);
  CHECK(pacer.invariants_hold());
  CHECK_EQ(rig.kmt().misuses(), 0u); }

/* A frame a refresh after the last, at the same place in its refresh, has a
 * blank between them however the lines compare. */
TEST(frame_pacer, a_frame_a_refresh_after_the_last_is_admitted_at_the_same_line)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.log(), rig.adapter(), rig.device());
  bool admitted[4] = {};
  admit_every(pacer, rig.kmt(), refresh, admitted);
  for (bool each : admitted) CHECK(each); }

/* Presents a sixteenth of a refresh short of one apart count as a blank apart
 * even within one refresh; a tick closer, and only the scan-out says. */
TEST(frame_pacer, presents_nearly_a_refresh_apart_count_as_a_blank_apart)
{ const std::uint64_t nearly = refresh - refresh / 16u;
  const std::uint64_t gaps[2] = {nearly - 1u, nearly};
  for (std::uint64_t gap : gaps) {
      PacerRig rig;
      FramePacer pacer(rig.kmt(), rig.log(), rig.adapter(), rig.device());
      rig.kmt().reads_clock(nullptr, 1000u);
      CHECK(pacer.admits());
      rig.kmt().reads_clock(nullptr, 5000u);
      CHECK(!pacer.admits());
      /* Still before the blank at `refresh`. */
      rig.kmt().reads_clock(nullptr, gap);
      CHECK_EQ(pacer.admits(), gap == nearly);
  } }

/* Where the scan-out cannot be read, frames keep a schedule a refresh apart
 * from the first. */
TEST(frame_pacer, without_an_adapter_frames_are_admitted_a_refresh_apart)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.log(), 0u, rig.device());
  bool admitted[9] = {};
  admit_every(pacer, rig.kmt(), 50000u, admitted);
  const bool expected[9] = {true, false, false, false, true, false, false, true, false};
  for (std::size_t i = 0u; i < 9u; i++) CHECK_EQ(admitted[i], expected[i]);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::scan_line), 0u); }

/* A refused read falls back to the schedule for that frame. */
TEST(frame_pacer, a_refused_scan_line_admits_on_the_schedule)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.log(), rig.adapter(), rig.device());
  rig.kmt().reads_clock(nullptr, 50000u);
  rig.kmt().fail_at(MockKmt::Call::scan_line, 1u, 0xC0000001u);
  CHECK(pacer.admits());
  rig.kmt().fail_at(MockKmt::Call::scan_line, 1u, 0xC0000001u);
  CHECK(!pacer.admits());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::scan_line), 2u); }
