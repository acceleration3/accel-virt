/* A Direct3D 11 application under a host GPU fault, over a scripted Direct3D
 * 11 and MockKmt playing the host that loses the device (HostLossRequest):
 * every step in order, the old device saying it was removed within the bound
 * and why, and a new one drawing while the old one is still held; a host
 * that keeps its device is a note, not a failure; a device never told, and a
 * new one that cannot draw, fail their steps. Both devices are released
 * whatever happened, and nothing the request opened is left open. */
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/probe/host_loss_request.hpp"
#include "guest/probe/host_loss_work.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d11_shared.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AvgpuAdapters;
using av::CommandHeader;
using av::GuestReport;
using av::HostLossRequest;
using av::HostLossWork;
using av::MockKmt;
using av::PciIdentity;
using av::ScenarioSteps;
using av::ScriptedD3d11Shared;
using av::Status;
using Call = av::ScriptedD3d11Shared::Call;

namespace {

const PciIdentity av_gpu(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);

/* The host losing the device: the first time the probe reads the clock after
 * the host answered, the old device is gone, as its process finds it. */
class Loser final : public av::ClockReader {
public:
    explicit Loser(ScriptedD3d11Shared &device) : device_(device), armed_(false) {}
    void arm() { armed_ = true; }
    void clock_read() override
    {
        if (armed_) device_.lose();
    }

private:
    ScriptedD3d11Shared &device_;
    bool                 armed_;
};

class LossRig {
public:
    explicit LossRig(std::uint32_t answer)
        : report_(report_memory_, "avd3d11probe"), steps_(kmt_, report_, "d3d11"), request_(kmt_), loser_(before_)
    {
        kmt_.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
        for (std::uint32_t at = 0u; at < 4u; at++) answer_[at] = static_cast<std::uint8_t>(answer >> (8u * at));
        kmt_.answer_waits(answer_, sizeof answer_);
    }
    LossRig(const LossRig &) = delete;
    LossRig &operator=(const LossRig &) = delete;

    MockKmt             &kmt() { return kmt_; }
    ScriptedD3d11Shared &before() { return before_; }
    ScriptedD3d11Shared &after() { return after_; }
    Loser               &loser() { return loser_; }

    const char *run()
    {
        { HostLossWork work(before_, after_, request_, kmt_);
          CHECK(work.invariants_hold());
          work.run(steps_);
          CHECK(work.invariants_hold()); }
        CHECK(!before_.live());
        CHECK(!after_.live());
        CHECK_EQ(kmt_.open_objects(), 0u);
        report_.finish();
        return report_.text();
    }

private:
    MockKmt             kmt_;
    av::MockPlatform report_memory_;
    GuestReport         report_{report_memory_};
    ScenarioSteps       steps_;
    ScriptedD3d11Shared before_;
    ScriptedD3d11Shared after_;
    HostLossRequest     request_;
    Loser               loser_;
    std::uint8_t        answer_[CommandHeader::lose_host_device_answer] = {};
};

bool has(const char *report, const char *line)
{
    return std::strstr(report, line) != nullptr;
}

}  // namespace

TEST(host_loss_work, the_old_device_is_removed_and_a_new_one_draws_while_it_is_held)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.kmt().reads_clock(&rig.loser(), 1u);
  rig.loser().arm();
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_device\n"));
  CHECK(has(report, "ok d3d11.loss_lose\n"));
  CHECK(has(report, "ok d3d11.loss_removed "));
  CHECK(has(report, "ok d3d11.loss_reason 2289696774\n"));   /* DXGI_ERROR_DEVICE_HUNG */
  CHECK(has(report, "ok d3d11.loss_recreated\n"));
  CHECK(has(report, "ok d3d11.loss_texel\n"));
  CHECK(has(report, "avd3d11probe: 6 passed, 0 failed"));
  /* The new device was made before the old one went. */
  CHECK_EQ(rig.after().calls(Call::create_device), 1u);
  CHECK_EQ(rig.before().calls(Call::release), 1u); }

/* A host started without the loss layer keeps its device: the probe says so
 * and stops, failing nothing, and never makes the second device. */
TEST(host_loss_work, a_host_that_keeps_its_device_is_a_note_and_nothing_more)
{ LossRig rig(CommandHeader::host_device_kept);
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_device\n"));
  CHECK(has(report, "note d3d11.loss_lose "));
  CHECK(!has(report, "loss_removed"));
  CHECK(has(report, "avd3d11probe: 1 passed, 0 failed"));
  CHECK_EQ(rig.after().calls(Call::create_device), 0u); }

/* A layer's device - DXVK beside the probe - runs on a host device of its own:
 * the probe says so and asks the host nothing. */
TEST(host_loss_work, a_layer_s_device_is_a_note_and_the_host_is_asked_nothing)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.before().set_layered(true);
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_device\n"));
  CHECK(has(report, "note d3d11.loss_lose "));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(rig.after().calls(Call::create_device), 0u); }

/* A device nobody told: every read still answers, the device still stands,
 * and the step fails once the bound has passed. */
TEST(host_loss_work, a_device_never_told_fails_once_the_bound_passes)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.kmt().set_frequency(1000u);
  rig.kmt().reads_clock(nullptr, 500u);
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_lose\n"));
  CHECK(has(report, "FAIL d3d11.loss_removed "));
  CHECK(!has(report, "loss_recreated"));
  CHECK(rig.before().calls(Call::read_texel) > 2u); }

/* The new device refused, or drawing the wrong colour, fails its step. */
TEST(host_loss_work, a_new_device_refused_or_drawing_wrong_fails)
{ { LossRig rig(CommandHeader::host_device_lost);
    rig.kmt().reads_clock(&rig.loser(), 1u);
    rig.loser().arm();
    rig.after().fail(Call::create_device, 1u, ScriptedD3d11Shared::device_removed);
    const char *const report = rig.run();
    CHECK(has(report, "FAIL d3d11.loss_recreated"));
    CHECK(!has(report, "loss_texel")); }
  { LossRig rig(CommandHeader::host_device_lost);
    rig.kmt().reads_clock(&rig.loser(), 1u);
    rig.loser().arm();
    rig.after().fail(Call::read_texel, 1u, ScriptedD3d11Shared::device_removed);
    const char *const report = rig.run();
    CHECK(has(report, "ok d3d11.loss_recreated\n"));
    CHECK(has(report, "FAIL d3d11.loss_texel")); } }

/* The first device refused stops everything: nothing is asked of the host. */
TEST(host_loss_work, a_first_device_refused_asks_the_host_nothing)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.before().fail(Call::create_device, 1u, ScriptedD3d11Shared::device_removed);
  const char *const report = rig.run();
  CHECK(has(report, "FAIL d3d11.loss_device"));
  CHECK(!has(report, "loss_lose"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 0u); }
