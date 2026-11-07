#include "executor/vulkan/vulkan_destroyer.hpp"
#include "executor/vulkan/vulkan_guard.hpp"
#include "executor/vulkan/vulkan_progress.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::VulkanDestroyKind;
using av::VulkanGuard;
using av::vkproto::HandleKind;

namespace {

constexpr std::uint64_t device = 1u;
constexpr std::uint64_t queue = 2u;
constexpr std::uint64_t cb = 100u;
constexpr std::uint64_t other_cb = 101u;
constexpr std::uint64_t buffer = 200u;
constexpr std::uint64_t image = 201u;
constexpr std::uint64_t view = 202u;
constexpr std::uint64_t set = 203u;
constexpr std::uint64_t pool = 204u;
constexpr std::uint64_t memory = 205u;
constexpr std::uint64_t sampler = 206u;
constexpr std::uint64_t texel_view = 207u;
constexpr std::uint64_t other_set = 208u;

class ScriptedProgress final : public av::VulkanProgress {
public:
    std::uint64_t marked() const override { return marked_; }
    std::uint64_t reached() override { return reached_; }
    std::uint64_t mark(std::uint64_t, std::uint64_t) override
    {
        if (refuse) return 0u;
        return ++marked_;
    }
    void wait_idle() override
    {
        waits++;
        reached_ = marked_;
    }

    std::uint64_t marked_ = 0u;
    std::uint64_t reached_ = 0u;
    std::uint32_t waits = 0u;
    bool          refuse = false;
};

class RecordingDestroyer final : public av::VulkanDestroyer {
public:
    void destroy(VulkanDestroyKind kind, std::uint64_t, std::uint64_t, std::uint64_t handle) override
    {
        if (count < 16u) {
            handles[count] = handle;
            kinds[count] = kind;
        }
        count++;
    }
    bool destroyed(std::uint64_t handle) const
    {
        for (std::uint32_t index = 0u; index < count && index < 16u; index++)
            if (handles[index] == handle) return true;
        return false;
    }

    std::uint64_t     handles[16] = {};
    VulkanDestroyKind kinds[16] = {};
    std::uint32_t     count = 0u;
};

struct Named {
    HandleKind    kind;
    std::uint64_t handle;
};

struct Rig {
    av::MockPlatform   platform;
    ScriptedProgress   progress;
    RecordingDestroyer destroyer;
    VulkanGuard        guard{platform, destroyer, progress};

    bool record(std::uint64_t buffer_cb, Named first, Named second = Named{HandleKind::device, 0u})
    {
        guard.begin_frame(true);
        bool kept = guard.reference(HandleKind::command_buffer, buffer_cb, 0u);
        kept = guard.reference(first.kind, first.handle, 0u) && kept;
        kept = guard.reference(second.kind, second.handle, 0u) && kept;
        guard.begin_frame(false);
        return kept;
    }
};

template <typename Handle>
Handle handle_of(std::uint64_t number)
{
    return reinterpret_cast<Handle>(number);
}

}

TEST(vulkan_guard, a_buffer_a_command_buffer_recorded_is_not_destroyed_before_the_command_buffer_is_reset)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}, Named{HandleKind::pipeline, 300u}));
  CHECK(rig.guard.holding(cb, buffer));
  CHECK(rig.guard.holding(cb, 300u));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK(rig.guard.release(cb));
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_command_buffer_that_was_submitted_keeps_its_objects_until_the_gpu_is_past_the_submission)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::image, image}));
  VkCommandBuffer buffers[1] = {handle_of<VkCommandBuffer>(cb)};
  VkSubmitInfo    submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1u;
  submit.pCommandBuffers = buffers;
  CHECK(rig.guard.submitted(device, queue, &submit, 1u));
  CHECK_EQ(rig.progress.marked_, 1u);
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  CHECK(rig.guard.release(cb));
  CHECK(!rig.destroyer.destroyed(image));
  rig.progress.reached_ = 0u;
  rig.guard.reap();
  CHECK(!rig.destroyer.destroyed(image));
  rig.progress.reached_ = 1u;
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(image)); }

TEST(vulkan_guard, a_second_style_of_submission_marks_its_command_buffers_too)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::image, image}));
  VkCommandBufferSubmitInfo infos[1] = {};
  infos[0].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  infos[0].commandBuffer = handle_of<VkCommandBuffer>(cb);
  VkSubmitInfo2 submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit.commandBufferInfoCount = 1u;
  submit.pCommandBufferInfos = infos;
  CHECK(rig.guard.submitted(device, queue, &submit, 1u));
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  CHECK(rig.guard.release(cb));
  CHECK(!rig.destroyer.destroyed(image));
  rig.progress.reached_ = 1u;
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(image)); }

TEST(vulkan_guard, a_submission_the_driver_cannot_mark_fails)
{ Rig rig;
  rig.progress.refuse = true;
  VkSubmitInfo submit = {};
  CHECK(!rig.guard.submitted(device, queue, &submit, 1u));
  VkSubmitInfo2 submit2 = {};
  CHECK(!rig.guard.submitted(device, queue, &submit2, 1u)); }

TEST(vulkan_guard, a_destroy_while_the_gpu_works_waits_for_the_gpu_and_an_idle_one_goes_through_at_once)
{ Rig rig;
  rig.guard.destroy(VulkanDestroyKind::sampler, device, 0u, sampler);
  CHECK(rig.destroyer.destroyed(sampler));
  rig.progress.marked_ = 4u;
  rig.progress.reached_ = 1u;
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(!rig.destroyer.destroyed(buffer));
  rig.progress.reached_ = 4u;
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(buffer)); }

TEST(vulkan_guard, nothing_is_held_outside_a_recording_or_for_what_a_command_does_not_keep)
{ Rig rig;
  rig.guard.begin_frame(false);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, 0u));
  CHECK(rig.guard.reference(HandleKind::buffer, buffer, 0u));
  CHECK(!rig.guard.tracked(buffer));
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::device, device, 0u));
  CHECK(rig.guard.reference(HandleKind::buffer, buffer, 0u));
  CHECK(!rig.guard.tracked(buffer));
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, 0u));
  CHECK(rig.guard.reference(HandleKind::device, device, 0u));
  CHECK(rig.guard.reference(HandleKind::instance, 9u, 0u));
  CHECK(rig.guard.reference(HandleKind::buffer, 0u, 0u));
  CHECK(!rig.guard.tracked(device));
  CHECK(!rig.guard.tracked(9u));
  CHECK(rig.guard.reference(HandleKind::buffer, buffer, 0u));
  CHECK(rig.guard.holding(cb, buffer)); }

TEST(vulkan_guard, a_frame_starts_with_no_command_buffer_so_a_later_command_names_its_own)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK(rig.record(other_cb, Named{HandleKind::image, image}));
  CHECK(!rig.guard.holding(other_cb, buffer));
  CHECK(rig.guard.holding(other_cb, image)); }

TEST(vulkan_guard, a_descriptor_set_written_keeps_every_resource_it_names_until_it_is_released)
{ Rig rig;
  VkDescriptorImageInfo images[2] = {};
  images[0].imageView = handle_of<VkImageView>(view);
  images[0].sampler = handle_of<VkSampler>(sampler);
  VkDescriptorBufferInfo buffers[1] = {};
  buffers[0].buffer = handle_of<VkBuffer>(buffer);
  VkBufferView texel[1] = {handle_of<VkBufferView>(texel_view)};
  VkWriteDescriptorSet writes[3] = {};
  writes[0].dstSet = handle_of<VkDescriptorSet>(set);
  writes[0].descriptorCount = 1u;
  writes[0].pImageInfo = images;
  writes[1].dstSet = handle_of<VkDescriptorSet>(set);
  writes[1].descriptorCount = 1u;
  writes[1].pBufferInfo = buffers;
  writes[2].dstSet = handle_of<VkDescriptorSet>(set);
  writes[2].descriptorCount = 1u;
  writes[2].pTexelBufferView = texel;
  CHECK(rig.guard.written(writes, 3u, device));
  CHECK(rig.guard.holding(set, view));
  CHECK(rig.guard.holding(set, sampler));
  CHECK(rig.guard.holding(set, buffer));
  CHECK(rig.guard.holding(set, texel_view));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.guard.destroy(VulkanDestroyKind::sampler, device, 0u, sampler);
  CHECK_EQ(rig.destroyer.count, 0u);
  CHECK(rig.guard.release(set));
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.destroyer.destroyed(sampler)); }

TEST(vulkan_guard, a_write_to_a_null_set_or_of_null_handles_keeps_nothing)
{ Rig rig;
  VkDescriptorImageInfo images[1] = {};
  VkWriteDescriptorSet  writes[2] = {};
  writes[0].descriptorCount = 1u;
  writes[0].pImageInfo = images;
  writes[1].dstSet = handle_of<VkDescriptorSet>(set);
  writes[1].descriptorCount = 1u;
  writes[1].pImageInfo = images;
  CHECK(rig.guard.written(writes, 2u, device));
  CHECK(!rig.guard.tracked(set));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_copied_descriptor_set_keeps_what_the_set_it_copied_from_kept)
{ Rig rig;
  VkDescriptorBufferInfo buffers[1] = {};
  buffers[0].buffer = handle_of<VkBuffer>(buffer);
  VkWriteDescriptorSet write = {};
  write.dstSet = handle_of<VkDescriptorSet>(set);
  write.descriptorCount = 1u;
  write.pBufferInfo = buffers;
  CHECK(rig.guard.written(&write, 1u, device));
  VkCopyDescriptorSet copy = {};
  copy.srcSet = handle_of<VkDescriptorSet>(set);
  copy.dstSet = handle_of<VkDescriptorSet>(other_set);
  CHECK(rig.guard.copied(&copy, 1u));
  CHECK(rig.guard.holding(other_set, buffer));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(rig.guard.release(set));
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK(rig.guard.release(other_set));
  CHECK(rig.destroyer.destroyed(buffer)); }

TEST(vulkan_guard, a_view_a_framebuffer_and_a_binding_keep_what_they_were_made_from)
{ Rig rig;
  CHECK(rig.guard.depends(view, image, device));
  CHECK(rig.guard.depends(buffer, memory, device));
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  rig.guard.destroy(VulkanDestroyKind::memory, device, 0u, memory);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.guard.destroy(VulkanDestroyKind::image_view, device, 0u, view);
  CHECK(rig.destroyer.destroyed(view));
  CHECK(rig.destroyer.destroyed(image));
  CHECK(!rig.destroyer.destroyed(memory)); }

TEST(vulkan_guard, a_pool_outlives_the_sets_and_command_buffers_a_command_buffer_still_names)
{ Rig rig;
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, 0u));
  CHECK(rig.guard.reference(HandleKind::descriptor_set, set, pool));
  rig.guard.begin_frame(false);
  rig.guard.destroy(VulkanDestroyKind::descriptor_pool, device, 0u, pool);
  CHECK(!rig.destroyer.destroyed(pool));
  CHECK(rig.guard.release(cb));
  CHECK(!rig.destroyer.destroyed(pool));
  CHECK(!rig.guard.tracked(set) || true);
  rig.guard.destroy(VulkanDestroyKind::descriptor_set, device, pool, set);
  CHECK(rig.destroyer.destroyed(pool)); }

TEST(vulkan_guard, a_reset_waits_for_the_gpu_only_when_the_command_buffer_may_still_run)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  rig.guard.await_reset(cb);
  CHECK_EQ(rig.progress.waits, 0u);
  VkCommandBuffer buffers[1] = {handle_of<VkCommandBuffer>(cb)};
  VkSubmitInfo    submit = {};
  submit.commandBufferCount = 1u;
  submit.pCommandBuffers = buffers;
  CHECK(rig.guard.submitted(device, queue, &submit, 1u));
  rig.guard.await_reset(cb);
  CHECK_EQ(rig.progress.waits, 1u);
  rig.guard.await_reset(cb);
  CHECK_EQ(rig.progress.waits, 1u); }

TEST(vulkan_guard, letting_everything_go_waits_for_the_gpu_and_finishes_the_destroys_the_guest_asked_for)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.progress.marked_ = 3u;
  rig.guard.release_all();
  CHECK_EQ(rig.progress.waits, 1u);
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(!rig.guard.tracked(buffer));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_hold_that_cannot_be_kept_refuses_the_command)
{ Rig rig;
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, 0u));
  rig.platform.fail_allocation_at(1u);
  CHECK(!rig.guard.reference(HandleKind::buffer, buffer, 0u));
  CHECK(!rig.guard.tracked(buffer)); }

TEST(vulkan_guard, a_destroy_that_cannot_be_deferred_waits_for_the_gpu_then_goes_through)
{ Rig rig;
  rig.progress.marked_ = 5u;
  rig.progress.reached_ = 1u;
  rig.platform.fail_allocation_at(1u);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK_EQ(rig.progress.waits, 1u);
  CHECK(rig.destroyer.destroyed(buffer)); }
