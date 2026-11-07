#include "harness/executor/fake_vulkan_driver.hpp"
#include "harness/executor/generous_budget.hpp"
#include "harness/executor/vulkan_wire_rig.hpp"
#include "harness/framework/test_registry.hpp"

namespace {

struct Rig : av::VulkanWireRig {
    Rig()
    {
        av::fake_vulkan = av::FakeVulkanDriver{};
        av::fake_vulkan.families = 1u;
        av::fake_vulkan.family_flags[0] = VK_QUEUE_GRAPHICS_BIT;
        av::fake_vulkan.fence_status = VK_NOT_READY;
        recorder = command_buffer(pool(0u));
    }

    std::uint64_t recorder = 0u;
};

}

TEST(vulkan_device_destroy, an_idle_device_is_destroyed_in_the_driver)
{ Rig rig;
  CHECK(rig.destroy_device());
  CHECK_EQ(av::fake_vulkan.device_destroys, 1u); }

TEST(vulkan_device_destroy, a_device_whose_work_finished_is_destroyed_in_the_driver)
{ Rig rig;
  CHECK(rig.submit(rig.queue, rig.recorder, rig.fence()));
  av::fake_vulkan.wait_result = VK_SUCCESS;
  CHECK(rig.destroy_device());
  CHECK_EQ(av::fake_vulkan.device_destroys, 1u); }

TEST(vulkan_device_destroy, a_device_whose_work_does_not_finish_is_leaked_not_destroyed)
{ Rig rig;
  CHECK(rig.submit(rig.queue, rig.recorder, rig.fence()));
  av::fake_vulkan.wait_result = VK_TIMEOUT;
  CHECK(rig.destroy_device());
  CHECK_EQ(av::fake_vulkan.device_destroys, 0u); }

TEST(vulkan_device_destroy, a_device_that_reports_loss_while_waited_on_is_leaked_not_destroyed)
{ Rig rig;
  CHECK(rig.submit(rig.queue, rig.recorder, rig.fence()));
  av::fake_vulkan.wait_result = VK_ERROR_DEVICE_LOST;
  CHECK(rig.destroy_device());
  CHECK_EQ(av::fake_vulkan.device_destroys, 0u);
  CHECK(rig.backend().lost()); }

TEST(vulkan_device_destroy, a_lost_device_is_never_counted_as_drained_so_its_deferred_destroys_never_run)
{ Rig rig;
  CHECK(rig.submit(rig.queue, rig.recorder, rig.fence()));
  av::fake_vulkan.fence_status = VK_ERROR_DEVICE_LOST;
  av::fake_vulkan.wait_result = VK_ERROR_DEVICE_LOST;
  CHECK(!rig.backend().lost());
  av::GenerousBudget budget;
  CHECK(!rig.backend().drain(budget.budget()));
  CHECK(rig.backend().lost()); }
