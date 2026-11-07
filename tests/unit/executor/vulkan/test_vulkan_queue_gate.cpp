#include "harness/executor/fake_vulkan_driver.hpp"
#include "harness/executor/vulkan_wire_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/vulkan/vk_command_names.h"

namespace {

constexpr VkQueueFlags universal = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

struct Rig : av::VulkanWireRig {
    Rig()
    {
        av::fake_vulkan = av::FakeVulkanDriver{};
        av::fake_vulkan.families = 5u;
        av::fake_vulkan.family_flags[0] = universal;
        av::fake_vulkan.family_flags[1] = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
        av::fake_vulkan.family_flags[2] = VK_QUEUE_TRANSFER_BIT;
        av::fake_vulkan.family_flags[3] = VK_QUEUE_COMPUTE_BIT;
        av::fake_vulkan.family_flags[4] = VK_QUEUE_GRAPHICS_BIT;
    }

    std::uint64_t recorder(std::uint32_t family) { return command_buffer(pool(family)); }
};

}

TEST(vulkan_queue_gate, the_registry_names_the_queues_each_command_may_be_recorded_for)
{ CHECK_EQ(av::vkproto::command_queues(av::vkproto::kCmd_vkCmdFillBuffer), 7u);
  CHECK_EQ(av::vkproto::command_queues(av::vkproto::kCmd_vkCmdDraw), 1u);
  CHECK_EQ(av::vkproto::command_queues(av::vkproto::kCmd_vkCmdDispatch), 2u);
  CHECK_EQ(av::vkproto::command_queues(av::vkproto::kCmd_vkCreateDevice), 0u); }

TEST(vulkan_queue_gate, a_fill_recorded_on_a_video_decode_command_buffer_is_refused_and_loses_the_context)
{ Rig rig;
  const std::uint64_t video = rig.recorder(1u);
  CHECK(video != 0u);
  CHECK(!rig.backend().lost());
  CHECK(rig.fill(video, rig.buffer()));
  CHECK_EQ(av::fake_vulkan.fills, 0u);
  CHECK(rig.backend().lost()); }

TEST(vulkan_queue_gate, a_fill_on_a_graphics_compute_or_transfer_command_buffer_reaches_the_driver)
{ Rig rig;
  const std::uint64_t target = rig.buffer();
  CHECK(rig.fill(rig.recorder(0u), target));
  CHECK(rig.fill(rig.recorder(2u), target));
  CHECK(rig.fill(rig.recorder(3u), target));
  CHECK(rig.fill(rig.recorder(4u), target));
  CHECK_EQ(av::fake_vulkan.fills, 4u);
  CHECK(!rig.backend().lost()); }

TEST(vulkan_queue_gate, a_draw_needs_a_graphics_family_and_a_dispatch_a_compute_family)
{ Rig rig;
  const std::uint64_t transfer = rig.recorder(2u);
  const std::uint64_t compute = rig.recorder(3u);
  const std::uint64_t graphics = rig.recorder(4u);
  CHECK(rig.draw(compute));
  CHECK_EQ(av::fake_vulkan.draws, 0u);
  CHECK(rig.backend().lost());
  Rig again;
  CHECK(again.dispatch(again.recorder(2u)));
  CHECK_EQ(av::fake_vulkan.dispatches, 0u);
  CHECK(again.backend().lost());
  Rig fine;
  CHECK(fine.draw(fine.recorder(4u)));
  CHECK(fine.dispatch(fine.recorder(3u)));
  CHECK_EQ(av::fake_vulkan.draws, 1u);
  CHECK_EQ(av::fake_vulkan.dispatches, 1u);
  CHECK(!fine.backend().lost());
  static_cast<void>(transfer);
  static_cast<void>(graphics); }

TEST(vulkan_queue_gate, a_query_reset_is_refused_on_a_transfer_only_command_buffer)
{ Rig rig;
  const std::uint64_t queries = rig.make(rig.device, av::vkproto::HandleKind::query_pool);
  CHECK(rig.reset_queries(rig.recorder(2u), queries));
  CHECK_EQ(av::fake_vulkan.query_resets, 0u);
  CHECK(rig.backend().lost());
  Rig fine;
  CHECK(fine.reset_queries(fine.recorder(3u), fine.make(fine.device, av::vkproto::HandleKind::query_pool)));
  CHECK_EQ(av::fake_vulkan.query_resets, 1u);
  CHECK(!fine.backend().lost()); }

TEST(vulkan_queue_gate, a_pool_for_a_family_the_device_does_not_have_is_refused_before_the_driver_is_asked)
{ Rig rig;
  CHECK_EQ(rig.pool(5u), 0u);
  CHECK_EQ(rig.pool(900u), 0u);
  CHECK_EQ(av::fake_vulkan.pools_made, 0u);
  CHECK(rig.pool(4u) != 0u);
  CHECK_EQ(av::fake_vulkan.pools_made, 1u); }

TEST(vulkan_queue_gate, a_command_on_a_destroyed_pool_s_buffer_never_reaches_the_driver)
{ Rig rig;
  const std::uint64_t pool = rig.pool(0u);
  const std::uint64_t recorder = rig.command_buffer(pool);
  av::WireWriter writer = rig.frame();
  writer.write_pod(rig.device);
  writer.write_pod(pool);
  CHECK(rig.send(av::vkproto::kCmd_vkDestroyCommandPool, writer));
  CHECK(!rig.fill(recorder, rig.buffer()));
  CHECK_EQ(av::fake_vulkan.fills, 0u); }

TEST(vulkan_queue_gate, a_command_buffer_of_a_pool_the_backend_never_saw_made_is_refused)
{ Rig rig;
  const std::uint64_t stranger = rig.make(rig.device, av::vkproto::HandleKind::command_pool);
  CHECK(rig.fill(rig.command_buffer(stranger), rig.buffer()));
  CHECK_EQ(av::fake_vulkan.fills, 0u);
  CHECK(rig.backend().lost()); }

TEST(vulkan_queue_gate, a_pool_on_a_device_with_no_physical_device_is_refused_before_the_driver_is_asked)
{ Rig rig;
  const std::uint64_t orphan = rig.make(0u, av::vkproto::HandleKind::device);
  CHECK_EQ(rig.pool(0u, orphan), 0u);
  CHECK_EQ(av::fake_vulkan.pools_made, 0u); }
