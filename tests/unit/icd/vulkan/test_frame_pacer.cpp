/* FIFO's pacing (feature 046, research.md §4): a presented frame is held until
 * the adapter's next vertical blank, and an adapter that cannot be waited on
 * leaves the frame unpaced - said once, however many frames follow. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "icd/vulkan/frame_pacer.hpp"

#include <cstdint>

using av::MockKmt;
using av::RecordingLog;
using av::icd::FramePacer;

namespace {

const char *const unpaced = "rvkvk: FIFO present unpaced";

av::PciIdentity rvk_identity()
{ return av::PciIdentity(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                         av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem); }

/* An adapter and a D3DKMT device on it, as the ICD's connection holds. */
class PacerRig {
public:
    PacerRig()
    { kmt_.add_adapter(rvk_identity(), av::AdapterScenario::wddm_3_2);
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
