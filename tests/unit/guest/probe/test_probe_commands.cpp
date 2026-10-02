/* One command buffer of the probe's over a scripted Vulkan: its pool on the
 * device's family, recording begun, and ended, submitted with its fence and
 * waited for; a refusal at each call stops there and is kept; a fence that
 * never signals is VK_TIMEOUT; the pool and fence go with this. */
#include "guest/suite/guest_report.hpp"
#include "guest/probe/probe_commands.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct ProbeCommandsProbe {
    static void poolless(ProbeCommands &commands) { commands.pool_ = VK_NULL_HANDLE; }
    static void fence_only(ProbeCommands &commands) { commands.pool_ = VK_NULL_HANDLE; commands.buffer_ = VK_NULL_HANDLE; }
};

}  // namespace av

using av::GuestReport;
using av::MockKmt;
using av::ProbeCommands;
using av::ProbeCommandsProbe;
using av::ProbeDevice;
using av::ProbeOutcome;
using av::ScenarioSteps;
using av::ScriptedVulkan;
using Call = av::ScriptedVulkan::Call;

namespace {

void make(ProbeDevice &device)
{
    CHECK_EQ(device.make_instance(), VK_SUCCESS);
    CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
    CHECK(device.choose(1u));
    CHECK(device.choose_family());
    CHECK_EQ(device.make_device(), VK_SUCCESS);
}

}  // namespace

TEST(probe_commands, a_buffer_is_recorded_submitted_and_waited_for)
{ ScriptedVulkan vulkan;
  { ProbeDevice device(vulkan);
    make(device);
    ProbeCommands commands(device);
    CHECK(commands.invariants_hold());
    CHECK(commands.buffer() == VK_NULL_HANDLE);
    ProbeOutcome outcome;
    CHECK(commands.begin(outcome));
    CHECK(commands.invariants_hold());
    CHECK(commands.buffer() != VK_NULL_HANDLE);
    CHECK_EQ(vulkan.calls(Call::begin_command_buffer), 1u);
    CHECK(commands.run(outcome));
    CHECK(commands.invariants_hold());
    CHECK_EQ(vulkan.calls(Call::end_command_buffer), 1u);
    CHECK_EQ(vulkan.calls(Call::queue_submit), 1u);
    CHECK_EQ(vulkan.calls(Call::wait_for_fence), 1u); }
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(probe_commands, a_refusal_while_beginning_stops_there)
{ const Call calls[] = {Call::create_command_pool, Call::allocate_command_buffer, Call::create_fence,
                        Call::begin_command_buffer};
  for (const Call call : calls) {
      ScriptedVulkan vulkan;
      vulkan.fail(call, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
      { ProbeDevice device(vulkan);
        make(device);
        ProbeCommands commands(device);
        ProbeOutcome outcome;
        CHECK(!commands.begin(outcome));
        CHECK(commands.invariants_hold());
        CHECK_EQ(vulkan.calls(Call::begin_command_buffer), call == Call::begin_command_buffer ? 1u : 0u); }
      CHECK_EQ(vulkan.live(), 0u);
      CHECK_EQ(vulkan.misuses(), 0u);
  } }

TEST(probe_commands, a_refusal_while_running_stops_there)
{ const Call calls[] = {Call::end_command_buffer, Call::queue_submit, Call::wait_for_fence};
  for (const Call call : calls) {
      ScriptedVulkan vulkan;
      vulkan.fail(call, 1u, VK_ERROR_DEVICE_LOST);
      { ProbeDevice device(vulkan);
        make(device);
        ProbeCommands commands(device);
        ProbeOutcome outcome;
        CHECK(commands.begin(outcome));
        CHECK(!commands.run(outcome));
        CHECK_EQ(vulkan.calls(Call::wait_for_fence), call == Call::wait_for_fence ? 1u : 0u); }
      CHECK_EQ(vulkan.live(), 0u);
  } }

/* A submission the host never completed: the fence is never signalled, and
 * the wait's VK_TIMEOUT is what the outcome reports. */
TEST(probe_commands, an_unsignalled_fence_times_out)
{ ScriptedVulkan vulkan;
  vulkan.hold_fences();
  MockKmt kmt;
  GuestReport report;
  ScenarioSteps steps(kmt, report, "vulkan");
  { ProbeDevice device(vulkan);
    make(device);
    ProbeCommands commands(device);
    ProbeOutcome outcome;
    CHECK(commands.begin(outcome));
    CHECK(!commands.run(outcome));
    outcome.report(steps, "work"); }
  CHECK(std::strcmp(report.text(), "FAIL vulkan.work status=7 2\n") == 0);
  CHECK_EQ(vulkan.live(), 0u); }

TEST(probe_commands, invariants_refuse_a_buffer_or_fence_without_a_pool)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  make(device);
  ProbeCommands buffered(device);
  ProbeOutcome outcome;
  CHECK(buffered.begin(outcome));
  ProbeCommandsProbe::poolless(buffered);
  CHECK(!buffered.invariants_hold());
  ProbeCommands fenced(device);
  CHECK(fenced.begin(outcome));
  ProbeCommandsProbe::fence_only(fenced);
  CHECK(!fenced.invariants_hold()); }
