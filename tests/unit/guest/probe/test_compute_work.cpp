#include "guest/probe/compute_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/probe/shaders/double_spv.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct ComputeWorkProbe {
    static void pipeline_without_layout(ComputeWork &work) { work.layout_ = VK_NULL_HANDLE; }
    static void layout_without_set_layout(ComputeWork &work) { work.set_layout_ = VK_NULL_HANDLE; }
    static void set_without_pool(ComputeWork &work) { work.pool_ = VK_NULL_HANDLE; }
};

}

using av::ComputeWork;
using av::ComputeWorkProbe;
using av::GuestReport;
using av::MockKmt;
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

class ComputeRig {
public:
    ComputeRig() : steps_(kmt_, report_, "vulkan") {}

    ScriptedVulkan &vulkan() { return vulkan_; }

    const char *run()
    {
        {
            ProbeDevice device(vulkan_);
            make(device);
            ComputeWork  work(device);
            ProbeOutcome outcome;
            CHECK(work.invariants_hold());
            work.run(outcome);
            CHECK(work.invariants_hold());
            outcome.report(steps_, "compute");
        }
        CHECK_EQ(vulkan_.live(), 0u);
        CHECK_EQ(vulkan_.misuses(), 0u);
        return report_.text();
    }

private:
    ScriptedVulkan vulkan_;
    MockKmt        kmt_;
    av::MockPlatform report_memory_;
    GuestReport    report_{report_memory_};
    ScenarioSteps  steps_;
};

}

TEST(compute_work, every_element_comes_back_doubled)
{ ComputeRig rig;
  CHECK(std::strcmp(rig.run(), "ok vulkan.compute 256\n") == 0);
  CHECK_EQ(rig.vulkan().dispatched_groups(), 16u);
  CHECK_EQ(rig.vulkan().shader_words(), av::double_spv_words);
  CHECK_EQ(rig.vulkan().calls(Call::queue_submit), 1u); }

TEST(compute_work, an_element_left_as_it_was_is_counted_out)
{ ComputeRig rig;
  rig.vulkan().miscompute();
  CHECK(std::strcmp(rig.run(), "FAIL vulkan.compute status=4 255\n") == 0); }

TEST(compute_work, a_submission_that_never_completes_times_out)
{ ComputeRig rig;
  rig.vulkan().hold_fences();
  CHECK(std::strcmp(rig.run(), "FAIL vulkan.compute status=7 2\n") == 0); }

TEST(compute_work, a_refusal_at_each_call_is_reported)
{ const Call calls[] = {Call::create_buffer,       Call::allocate_memory,   Call::bind_buffer_memory,
                        Call::map_memory,          Call::create_shader_module, Call::create_descriptor_set_layout,
                        Call::create_pipeline_layout, Call::create_compute_pipeline, Call::create_descriptor_pool,
                        Call::allocate_descriptor_set, Call::create_command_pool, Call::queue_submit};
  for (const Call call : calls) {
      ComputeRig rig;
      rig.vulkan().fail(call, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
      CHECK(std::strcmp(rig.run(), "FAIL vulkan.compute status=7 4294967295\n") == 0);
  } }

TEST(compute_work, a_buffer_with_no_host_visible_type_is_missing)
{ ComputeRig rig;
  rig.vulkan().set_allowed_types(0x1u, 0x1u);
  CHECK(std::strcmp(rig.run(), "FAIL vulkan.compute status=3 0\n") == 0);
  CHECK_EQ(rig.vulkan().calls(Call::create_shader_module), 0u); }

TEST(compute_work, invariants_refuse_objects_without_what_they_stand_on)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  make(device);
  ComputeWork one(device);
  ProbeOutcome outcome;
  one.run(outcome);
  ComputeWorkProbe::pipeline_without_layout(one);
  CHECK(!one.invariants_hold());
  ComputeWork two(device);
  two.run(outcome);
  ComputeWorkProbe::layout_without_set_layout(two);
  CHECK(!two.invariants_hold());
  ComputeWork three(device);
  three.run(outcome);
  ComputeWorkProbe::set_without_pool(three);
  CHECK(!three.invariants_hold()); }
