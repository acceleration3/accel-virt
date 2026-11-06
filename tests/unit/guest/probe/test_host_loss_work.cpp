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

namespace av {

struct HostLossWorkProbe {
    static void a_new_device_without_an_old_one(HostLossWork &work) { work.made_after_ = true; }
};

}

namespace {

const PciIdentity av_gpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);

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

}

TEST(host_loss_work, the_old_device_is_removed_and_a_new_one_draws_while_it_is_held)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.kmt().reads_clock(&rig.loser(), 1u);
  rig.loser().arm();
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_device\n"));
  CHECK(has(report, "ok d3d11.loss_lose\n"));
  CHECK(has(report, "ok d3d11.loss_removed "));
  CHECK(has(report, "ok d3d11.loss_reason 2289696774\n"));
  CHECK(has(report, "ok d3d11.loss_recreated\n"));
  CHECK(has(report, "ok d3d11.loss_texel\n"));
  CHECK(has(report, "avd3d11probe: 6 passed, 0 failed"));
  CHECK_EQ(rig.after().calls(Call::create_device), 1u);
  CHECK_EQ(rig.before().calls(Call::release), 1u); }

TEST(host_loss_work, a_host_that_keeps_its_device_is_a_note_and_nothing_more)
{ LossRig rig(CommandHeader::host_device_kept);
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_device\n"));
  CHECK(has(report, "note d3d11.loss_lose "));
  CHECK(!has(report, "loss_removed"));
  CHECK(has(report, "avd3d11probe: 1 passed, 0 failed"));
  CHECK_EQ(rig.after().calls(Call::create_device), 0u); }

TEST(host_loss_work, a_layer_s_device_is_a_note_and_the_host_is_asked_nothing)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.before().set_layered(true);
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_device\n"));
  CHECK(has(report, "note d3d11.loss_lose "));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(rig.after().calls(Call::create_device), 0u); }

TEST(host_loss_work, a_device_never_told_fails_once_the_bound_passes)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.kmt().set_frequency(1000u);
  rig.kmt().reads_clock(nullptr, 500u);
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_lose\n"));
  CHECK(has(report, "FAIL d3d11.loss_removed "));
  CHECK(!has(report, "loss_recreated"));
  CHECK(rig.before().calls(Call::read_texel) > 2u); }

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

TEST(host_loss_work, a_first_device_refused_asks_the_host_nothing)
{ LossRig rig(CommandHeader::host_device_lost);
  rig.before().fail(Call::create_device, 1u, ScriptedD3d11Shared::device_removed);
  const char *const report = rig.run();
  CHECK(has(report, "FAIL d3d11.loss_device"));
  CHECK(!has(report, "loss_lose"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 0u); }

TEST(host_loss_work, a_new_device_without_an_old_one_is_an_impossible_state)
{ ScriptedD3d11Shared before;
  ScriptedD3d11Shared after;
  MockKmt             kmt;
  HostLossRequest     request(kmt);
  HostLossWork        work(before, after, request, kmt);
  CHECK(work.invariants_hold());
  av::HostLossWorkProbe::a_new_device_without_an_old_one(work);
  CHECK(!work.invariants_hold());
  av::HostLossWorkProbe::a_new_device_without_an_old_one(work); }

TEST(host_loss_work, a_target_the_device_cannot_make_or_a_texel_of_the_wrong_colour_fails_the_draw)
{ { LossRig rig(CommandHeader::host_device_lost);
    rig.before().fail(Call::create_target, 1u, ScriptedD3d11Shared::device_removed);
    const char *const report = rig.run();
    CHECK(has(report, "FAIL d3d11.loss_device"));
    CHECK(!has(report, "loss_lose")); }
  { LossRig rig(CommandHeader::host_device_lost);
    rig.kmt().reads_clock(&rig.loser(), 1u);
    rig.loser().arm();
    rig.after().fail(Call::create_target, 1u, ScriptedD3d11Shared::device_removed);
    const char *const report = rig.run();
    CHECK(has(report, "ok d3d11.loss_recreated\n"));
    CHECK(has(report, "FAIL d3d11.loss_texel")); }
  const auto wrong = [](void (*spoil)(ScriptedD3d11Shared &)) {
      LossRig rig(CommandHeader::host_device_lost);
      rig.kmt().reads_clock(&rig.loser(), 1u);
      rig.loser().arm();
      spoil(rig.after());
      const char *const report = rig.run();
      CHECK(has(report, "ok d3d11.loss_recreated\n"));
      CHECK(has(report, "FAIL d3d11.loss_texel status=7 2147500037\n"));
  };
  wrong(+[](ScriptedD3d11Shared &device) { device.misplace_rows(); });
  wrong(+[](ScriptedD3d11Shared &device) { device.overshoot_rows(); });
  wrong(+[](ScriptedD3d11Shared &device) { device.misplace_one_channel(0u); });
  wrong(+[](ScriptedD3d11Shared &device) { device.misplace_one_channel(1u); });
  wrong(+[](ScriptedD3d11Shared &device) { device.misplace_one_channel(2u); }); }

TEST(host_loss_work, a_host_that_answers_something_else_fails_the_lose_step)
{ LossRig rig(0xDEADu);
  const char *const report = rig.run();
  CHECK(has(report, "ok d3d11.loss_device\n"));
  CHECK(has(report, "FAIL d3d11.loss_lose"));
  CHECK(!has(report, "loss_removed")); }

TEST(host_loss_work, an_old_device_is_removed_when_either_its_texel_or_its_reason_says_so)
{ { LossRig rig(CommandHeader::host_device_lost);
    rig.before().fail(Call::removed_reason, 1u, ScriptedD3d11Shared::device_removed);
    const char *const report = rig.run();
    CHECK(has(report, "ok d3d11.loss_removed "));
    CHECK(has(report, "ok d3d11.loss_reason 2289696773\n")); }
  { LossRig rig(CommandHeader::host_device_lost);
    rig.before().fail(Call::read_texel, 2u, ScriptedD3d11Shared::device_removed);
    const char *const report = rig.run();
    CHECK(has(report, "ok d3d11.loss_removed "));
    CHECK(has(report, "FAIL d3d11.loss_reason")); } }

TEST(host_loss_work, a_clock_without_a_frequency_measures_no_time_and_one_far_ahead_measures_the_most_it_can)
{ { LossRig rig(CommandHeader::host_device_lost);
    rig.kmt().set_frequency(0u);
    rig.kmt().reads_clock(&rig.loser(), 1u);
    rig.loser().arm();
    const char *const report = rig.run();
    CHECK(has(report, "ok d3d11.loss_removed 0\n"));
    CHECK(has(report, "ok d3d11.loss_texel\n")); }
  { LossRig rig(CommandHeader::host_device_lost);
    rig.kmt().set_frequency(1000u);
    rig.kmt().reads_clock(nullptr, 10000000000000ull);
    const char *const report = rig.run();
    CHECK(has(report, "FAIL d3d11.loss_removed status=7 4294967295\n")); } }
