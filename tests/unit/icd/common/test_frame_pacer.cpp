#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "core/log/driver_log.hpp"
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

const char *const unpaced = "presents unpaced";

av::PciIdentity av_identity()
{ return av::PciIdentity(av::PciIdentity::virtio_vendor, av::AvgpuAdapters::virtio_gpu_device,
                         av::PciIdentity::virtio_vendor, av::AvgpuAdapters::av_subsystem); }

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
    av::DriverLog &lines() { return lines_; }
    std::uint32_t adapter() const { return adapter_; }
    std::uint32_t device() const { return device_; }

private:
    MockKmt       kmt_;
    RecordingLog  log_;
    av::DriverLog lines_{log_, "avgl: "};
    std::uint32_t adapter_ = 0u;
    std::uint32_t device_ = 0u;
};

}

TEST(frame_pacer, a_wait_counts_one_blank_on_the_adapter_s_primary_source)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
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

TEST(frame_pacer, a_refused_wait_is_said_once_and_paces_nothing)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
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
  pacer.wait();
  CHECK_EQ(pacer.blanks(), 1u);
  CHECK_EQ(pacer.failures(), 2u); }

TEST(frame_pacer, no_adapter_means_no_wait_at_all)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), 0u, rig.device());
  pacer.wait();
  CHECK(pacer.invariants_hold());
  CHECK_EQ(pacer.blanks(), 0u);
  CHECK_EQ(pacer.failures(), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 0u);
  CHECK_EQ(rig.log().holding(unpaced), 1u); }

TEST(frame_pacer, a_refused_wait_is_said_in_the_name_of_the_driver_that_paces)
{ PacerRig rig;
  av::DriverLog vulkan(rig.log(), "avvk: ");
  FramePacer gl_pacer(rig.kmt(), rig.lines(), 0u, rig.device());
  gl_pacer.wait();
  CHECK_EQ(rig.log().holding("avgl: presents unpaced"), 1u);
  CHECK_EQ(rig.log().holding("avvk: "), 0u);
  FramePacer vk_pacer(rig.kmt(), vulkan, 0u, rig.device());
  vk_pacer.wait();
  CHECK_EQ(rig.log().holding("avvk: presents unpaced"), 1u);
  CHECK_EQ(rig.log().holding("FIFO"), 0u); }

namespace {
constexpr std::uint64_t refresh = MockKmt::ticks_per_second / 60u;
constexpr std::uint64_t first_present = 2u * refresh + 100000u;

template <std::size_t count>
void admit_every(FramePacer &pacer, MockKmt &kmt, std::uint64_t step, bool (&admitted)[count])
{ kmt.reads_clock(nullptr, first_present);
  admitted[0] = pacer.admits();
  kmt.reads_clock(nullptr, step);
  for (std::size_t i = 1u; i < count; i++) admitted[i] = pacer.admits(); }

}

TEST(frame_pacer, the_first_frame_presented_after_each_blank_is_admitted)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
  bool admitted[9] = {};
  admit_every(pacer, rig.kmt(), 50000u, admitted);
  const bool expected[9] = {true, false, true, false, false, true, false, false, true};
  for (std::size_t i = 0u; i < 9u; i++) CHECK_EQ(admitted[i], expected[i]);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::scan_line), 9u);
  CHECK(pacer.invariants_hold());
  CHECK_EQ(rig.kmt().misuses(), 0u); }

TEST(frame_pacer, a_frame_a_refresh_after_the_last_is_admitted_at_the_same_line)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
  bool admitted[4] = {};
  admit_every(pacer, rig.kmt(), refresh, admitted);
  for (bool each : admitted) CHECK(each); }

TEST(frame_pacer, presents_nearly_a_refresh_apart_count_as_a_blank_apart)
{ const std::uint64_t nearly = refresh - refresh / 16u;
  const std::uint64_t gaps[2] = {nearly - 1u, nearly};
  for (std::uint64_t gap : gaps) {
      PacerRig rig;
      FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
      rig.kmt().reads_clock(nullptr, 1000u);
      CHECK(pacer.admits());
      rig.kmt().reads_clock(nullptr, 5000u);
      CHECK(!pacer.admits());
      rig.kmt().reads_clock(nullptr, gap);
      CHECK_EQ(pacer.admits(), gap == nearly);
  } }

TEST(frame_pacer, without_an_adapter_frames_are_admitted_a_refresh_apart)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), 0u, rig.device());
  bool admitted[9] = {};
  admit_every(pacer, rig.kmt(), 50000u, admitted);
  const bool expected[9] = {true, false, false, false, true, false, false, true, false};
  for (std::size_t i = 0u; i < 9u; i++) CHECK_EQ(admitted[i], expected[i]);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::scan_line), 0u); }

TEST(frame_pacer, a_refused_scan_line_admits_on_the_schedule)
{ PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
  rig.kmt().reads_clock(nullptr, 50000u);
  rig.kmt().fail_at(MockKmt::Call::scan_line, 1u, 0xC0000001u);
  CHECK(pacer.admits());
  rig.kmt().fail_at(MockKmt::Call::scan_line, 1u, 0xC0000001u);
  CHECK(!pacer.admits());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::scan_line), 2u); }

TEST(frame_pacer, presents_nearly_a_refresh_apart_at_the_adapter_s_own_refresh_count_as_a_blank_apart)
{ constexpr std::uint64_t fast = MockKmt::ticks_per_second / 240u;
  const std::uint64_t     nearly = fast - fast / 16u;
  const std::uint64_t     gaps[2] = {nearly - 1u, nearly};
  for (std::uint64_t gap : gaps) {
      PacerRig rig;
      rig.kmt().set_blank_period(fast);
      FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
      rig.kmt().reads_clock(nullptr, 100u);
      CHECK(pacer.admits());
      rig.kmt().reads_clock(nullptr, 300u);
      CHECK(!pacer.admits());
      rig.kmt().reads_clock(nullptr, gap);
      CHECK_EQ(pacer.admits(), gap == nearly);
  } }

TEST(frame_pacer, a_refresh_changed_while_running_is_read_again_within_a_second)
{ constexpr std::uint64_t fast = MockKmt::ticks_per_second / 240u;
  PacerRig rig;
  FramePacer pacer(rig.kmt(), rig.lines(), rig.adapter(), rig.device());
  const auto admits = [&](std::uint64_t step) {
      rig.kmt().reads_clock(nullptr, step);
      rig.kmt().fail_at(MockKmt::Call::scan_line, 1u, 0xC0000001u);
      return pacer.admits();
  };
  CHECK(admits(first_present));
  rig.kmt().set_blank_period(fast);
  CHECK(admits(MockKmt::ticks_per_second / 2u));
  CHECK(!admits(fast + fast / 2u));
  CHECK(admits(MockKmt::ticks_per_second / 2u));
  CHECK(!admits(fast / 2u));
  CHECK(admits(fast / 2u + fast / 10u));
  CHECK(pacer.invariants_hold()); }
