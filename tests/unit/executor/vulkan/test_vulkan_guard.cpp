#include "harness/executor/generous_budget.hpp"
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
constexpr std::uint64_t other_device = 2u;
constexpr std::uint64_t queue = 3u;
constexpr std::uint64_t cb = 100u;
constexpr std::uint64_t other_cb = 101u;
constexpr std::uint64_t secondary = 102u;
constexpr std::uint64_t buffer = 200u;
constexpr std::uint64_t image = 201u;
constexpr std::uint64_t view = 202u;
constexpr std::uint64_t set = 203u;
constexpr std::uint64_t pool = 204u;
constexpr std::uint64_t memory = 205u;
constexpr std::uint64_t sampler = 206u;
constexpr std::uint64_t texel_view = 207u;
constexpr std::uint64_t other_set = 208u;
constexpr std::uint64_t command_pool = 209u;
constexpr std::uint64_t other_view = 210u;
constexpr std::uint64_t structure = 211u;
constexpr std::uint64_t slots = 16u;

class ScriptedProgress final : public av::VulkanProgress {
public:
    std::uint64_t marked() const override { return marked_; }
    bool          lost() const override { return device_lost; }
    std::uint64_t reached(std::uint64_t of) override { return of < slots ? reached_[of] : 0u; }
    std::uint64_t reserve(std::uint64_t of) override
    {
        reserved_for = of;
        if (refuse || pending == capacity) return 0u;
        pending++;
        return ++marked_;
    }
    void commit(std::uint64_t mark, std::uint64_t on) override
    {
        committed = mark;
        committed_on = on;
        commits++;
    }
    void cancel(std::uint64_t) override
    {
        cancels++;
        pending--;
    }
    bool device_lost = false;
    bool wait_idle(std::uint64_t of) override
    {
        waits++;
        if (timeout) return false;
        if (of < slots) reached_[of] = marked_;
        pending = 0u;
        return true;
    }
    bool wait_marks(std::uint64_t of, const std::uint64_t *marks, std::uint32_t count) override
    {
        waits++;
        waited_marks = count;
        std::uint64_t newest = 0u;
        for (std::uint32_t index = 0u; index < count; index++) {
            waited_through = marks[index];
            if (marks[index] > newest) newest = marks[index];
        }
        if (timeout) return false;
        const std::uint64_t to = newest < marked_ ? newest : marked_;
        if (of < slots && reached_[of] < to) reached_[of] = to;
        return true;
    }
    bool drain(std::uint64_t of, av::TeardownBudget &) override
    {
        waits++;
        if (timeout) return false;
        if (of < slots) reached_[of] = marked_;
        return true;
    }
    void begin_poll() override { polls++; }

    void reach(std::uint64_t of, std::uint64_t mark) { reached_[of] = mark; }
    void reach_all(std::uint64_t mark)
    {
        for (std::uint64_t index = 0u; index < slots; index++) reached_[index] = mark;
    }

    std::uint64_t marked_ = 0u;
    std::uint64_t reached_[slots] = {};
    std::uint64_t reserved_for = 0u;
    std::uint64_t committed = 0u;
    std::uint64_t committed_on = 0u;
    std::uint64_t waited_through = 0u;
    std::uint32_t commits = 0u;
    std::uint32_t cancels = 0u;
    std::uint32_t waits = 0u;
    std::uint32_t polls = 0u;
    std::uint32_t pending = 0u;
    std::uint32_t capacity = 128u;
    std::uint32_t waited_marks = 0u;
    bool          refuse = false;
    bool          timeout = false;
};

class RecordingDestroyer final : public av::VulkanDestroyer {
public:
    void destroy(VulkanDestroyKind kind, std::uint64_t of, std::uint64_t parent_of, std::uint64_t handle) override
    {
        if (count < 16u) {
            handles[count] = handle;
            kinds[count] = kind;
            parents[count] = parent_of;
            devices[count] = of;
        }
        count++;
    }
    bool destroyed(std::uint64_t handle) const
    {
        for (std::uint32_t index = 0u; index < count && index < 16u; index++)
            if (handles[index] == handle) return true;
        return false;
    }
    std::uint32_t order_of(std::uint64_t handle) const
    {
        for (std::uint32_t index = 0u; index < count && index < 16u; index++)
            if (handles[index] == handle) return index;
        return 16u;
    }

    std::uint64_t     handles[16] = {};
    std::uint64_t     parents[16] = {};
    std::uint64_t     devices[16] = {};
    VulkanDestroyKind kinds[16] = {};
    std::uint32_t     count = 0u;
};

struct Named {
    HandleKind    kind;
    std::uint64_t handle;
    std::uint64_t parent = 0u;
};

struct Rig {
    av::MockPlatform   platform;
    ScriptedProgress   progress;
    RecordingDestroyer destroyer;
    VulkanGuard        guard{platform, destroyer, progress};

    bool record(std::uint64_t buffer_cb, Named first, Named second = Named{HandleKind::device, 0u},
                std::uint64_t on = device, std::uint64_t buffer_pool = 0u)
    {
        guard.begin_frame(true);
        bool kept = guard.reference(HandleKind::command_buffer, buffer_cb, buffer_pool, on);
        kept = guard.reference(first.kind, first.handle, first.parent, 0u) && kept;
        kept = guard.reference(second.kind, second.handle, second.parent, 0u) && kept;
        guard.begin_frame(false);
        return kept;
    }

    std::uint64_t submit(std::uint64_t buffer_cb, std::uint64_t on = device)
    {
        const std::uint64_t mark = guard.reserve(on);
        VkCommandBuffer     buffers[1];
        buffers[0] = reinterpret_cast<VkCommandBuffer>(buffer_cb);
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1u;
        submit_info.pCommandBuffers = buffers;
        if (mark != 0u) guard.submitted(mark, on, queue, &submit_info, 1u);
        return mark;
    }

    void free_command_buffer(std::uint64_t buffer_cb, std::uint64_t buffer_pool, std::uint64_t on = device)
    {
        static_cast<void>(guard.release(buffer_cb, on));
        guard.destroy(VulkanDestroyKind::command_buffer, on, buffer_pool, buffer_cb);
    }

    void free_descriptor_set(std::uint64_t freed, std::uint64_t from, std::uint64_t on = device)
    {
        static_cast<void>(guard.release(freed, on));
        guard.destroy(VulkanDestroyKind::descriptor_set, on, from, freed);
    }

    void destroy_pool(VulkanDestroyKind kind, std::uint64_t doomed, std::uint64_t first_child, std::uint64_t second_child,
                      std::uint64_t on = device)
    {
        if (first_child != 0u) static_cast<void>(guard.release(first_child, on));
        if (second_child != 0u) static_cast<void>(guard.release(second_child, on));
        guard.orphan_children(doomed, on);
        guard.destroy(kind, on, 0u, doomed);
    }

    bool reset_descriptor_pool(std::uint64_t reset, std::uint64_t first_child, std::uint64_t second_child,
                               std::uint64_t on = device)
    {
        const std::uint64_t mark = guard.mark_of(first_child, on);
        if (!guard.quiesce_marks(&mark, 1u, on)) return false;
        if (first_child != 0u) static_cast<void>(guard.release(first_child, on));
        if (second_child != 0u) static_cast<void>(guard.release(second_child, on));
        guard.orphan_children(reset, on);
        return true;
    }

    bool begin(std::uint64_t buffer_cb, std::uint64_t on = device)
    {
        return guard.await_reset(buffer_cb, on) && guard.release(buffer_cb, on);
    }
};

template <typename Handle>
Handle handle_of(std::uint64_t number)
{
    return reinterpret_cast<Handle>(number);
}

VkWriteDescriptorSet write_of(std::uint64_t target, VkDescriptorType type, std::uint32_t binding, std::uint32_t element,
                              std::uint32_t count)
{
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = handle_of<VkDescriptorSet>(target);
    write.dstBinding = binding;
    write.dstArrayElement = element;
    write.descriptorCount = count;
    write.descriptorType = type;
    return write;
}

}

TEST(vulkan_guard, a_buffer_a_command_buffer_recorded_is_not_destroyed_before_the_command_buffer_is_reset)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}, Named{HandleKind::pipeline, 300u}));
  CHECK(rig.guard.holding(cb, buffer));
  CHECK(rig.guard.holding(cb, 300u));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK(rig.guard.release(cb, device));
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_command_buffer_that_was_submitted_keeps_its_objects_until_the_gpu_is_past_the_submission)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::image, image}));
  CHECK_EQ(rig.submit(cb), 1u);
  CHECK_EQ(rig.progress.committed, 1u);
  CHECK_EQ(rig.progress.committed_on, queue);
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  CHECK(rig.guard.release(cb, device));
  CHECK(!rig.destroyer.destroyed(image));
  rig.guard.reap();
  CHECK(!rig.destroyer.destroyed(image));
  rig.progress.reach(device, 1u);
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
  const std::uint64_t mark = rig.guard.reserve(device);
  CHECK_EQ(mark, 1u);
  rig.guard.submitted(mark, device, queue, &submit, 1u);
  CHECK_EQ(rig.progress.commits, 1u);
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  CHECK(rig.guard.release(cb, device));
  CHECK(!rig.destroyer.destroyed(image));
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(image)); }

TEST(vulkan_guard, a_submission_without_a_mark_to_give_is_refused_before_anything_is_submitted_or_marked)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::image, image}));
  rig.progress.refuse = true;
  CHECK_EQ(rig.guard.reserve(device), 0u);
  CHECK_EQ(rig.submit(cb), 0u);
  CHECK_EQ(rig.progress.commits, 0u);
  CHECK(rig.guard.await_reset(cb, device));
  CHECK_EQ(rig.progress.waits, 0u); }

TEST(vulkan_guard, a_hundred_and_twenty_eight_pending_submissions_leave_the_next_one_refused_whatever_waits_were_made)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::image, image}));
  for (std::uint32_t index = 0u; index < 128u; index++) CHECK_EQ(rig.submit(cb), index + 1u);
  CHECK_EQ(rig.submit(cb), 0u);
  CHECK_EQ(rig.progress.commits, 128u);
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  CHECK(!rig.destroyer.destroyed(image));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_submission_the_driver_fails_is_cancelled_and_marks_nothing)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::image, image}));
  const std::uint64_t mark = rig.guard.reserve(device);
  CHECK_EQ(mark, 1u);
  rig.guard.cancel(mark);
  CHECK_EQ(rig.progress.cancels, 1u);
  CHECK_EQ(rig.progress.commits, 0u);
  CHECK(rig.guard.await_reset(cb, device));
  CHECK_EQ(rig.progress.waits, 0u);
  CHECK_EQ(rig.guard.reserve(device), 2u); }

TEST(vulkan_guard, a_reservation_for_a_device_the_guard_cannot_track_is_refused)
{ Rig rig;
  rig.platform.fail_allocation_at(1u);
  CHECK_EQ(rig.guard.reserve(device), 0u);
  CHECK_EQ(rig.progress.reserved_for, 0u); }

TEST(vulkan_guard, a_destroy_while_the_gpu_works_waits_for_the_gpu_and_an_idle_one_goes_through_at_once)
{ Rig rig;
  rig.guard.destroy(VulkanDestroyKind::sampler, device, 0u, sampler);
  CHECK(rig.destroyer.destroyed(sampler));
  rig.progress.marked_ = 4u;
  rig.progress.reach(device, 1u);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(!rig.destroyer.destroyed(buffer));
  rig.progress.reach(device, 4u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(buffer)); }

TEST(vulkan_guard, one_device_s_blocked_work_does_not_hold_back_another_device_s_destroys)
{ Rig rig;
  rig.progress.marked_ = 6u;
  rig.progress.reach(device, 2u);
  rig.progress.reach(other_device, 6u);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.guard.destroy(VulkanDestroyKind::image, other_device, 0u, image);
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK(rig.destroyer.destroyed(image));
  rig.guard.reap();
  CHECK(!rig.destroyer.destroyed(buffer));
  rig.progress.reach(device, 6u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK_EQ(rig.destroyer.devices[rig.destroyer.order_of(image)], other_device);
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, nothing_is_held_outside_a_recording_or_for_what_a_command_does_not_keep)
{ Rig rig;
  rig.guard.begin_frame(false);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, 0u, device));
  CHECK(rig.guard.reference(HandleKind::buffer, buffer, 0u, 0u));
  CHECK(!rig.guard.tracked(buffer));
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::device, device, 0u, 0u));
  CHECK(rig.guard.reference(HandleKind::buffer, buffer, 0u, 0u));
  CHECK(!rig.guard.tracked(buffer));
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, 0u, device));
  CHECK(rig.guard.reference(HandleKind::device, device, 0u, 0u));
  CHECK(rig.guard.reference(HandleKind::instance, 9u, 0u, 0u));
  CHECK(rig.guard.reference(HandleKind::buffer, 0u, 0u, 0u));
  CHECK(!rig.guard.tracked(device));
  CHECK(!rig.guard.tracked(9u));
  CHECK(rig.guard.reference(HandleKind::buffer, buffer, 0u, 0u));
  CHECK(rig.guard.holding(cb, buffer)); }

TEST(vulkan_guard, a_frame_starts_with_no_command_buffer_so_a_later_command_names_its_own)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK(rig.record(other_cb, Named{HandleKind::image, image}));
  CHECK(!rig.guard.holding(other_cb, buffer));
  CHECK(rig.guard.holding(other_cb, image)); }

TEST(vulkan_guard, the_gpu_is_polled_once_per_batch_however_many_commands_the_batch_holds)
{ Rig rig;
  rig.guard.begin_frame(false);
  rig.guard.begin_frame(true);
  rig.guard.begin_frame(true);
  CHECK_EQ(rig.progress.polls, 0u);
  rig.guard.end_batch();
  CHECK_EQ(rig.progress.polls, 1u);
  rig.guard.end_batch();
  CHECK_EQ(rig.progress.polls, 2u); }

TEST(vulkan_guard, a_descriptor_set_written_keeps_every_resource_it_names_until_it_is_released)
{ Rig rig;
  VkDescriptorImageInfo images[2] = {};
  images[0].imageView = handle_of<VkImageView>(view);
  images[0].sampler = handle_of<VkSampler>(sampler);
  VkDescriptorBufferInfo buffers[1] = {};
  buffers[0].buffer = handle_of<VkBuffer>(buffer);
  VkBufferView texel[1] = {handle_of<VkBufferView>(texel_view)};
  VkWriteDescriptorSet writes[3] = {write_of(set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0u, 0u, 1u),
                                    write_of(set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, 0u, 1u),
                                    write_of(set, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2u, 0u, 1u)};
  writes[0].pImageInfo = images;
  writes[1].pBufferInfo = buffers;
  writes[2].pTexelBufferView = texel;
  CHECK(rig.guard.written(writes, 3u, device));
  CHECK(rig.guard.holding(set, view));
  CHECK(rig.guard.holding(set, sampler));
  CHECK(rig.guard.holding(set, buffer));
  CHECK(rig.guard.holding(set, texel_view));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.guard.destroy(VulkanDestroyKind::sampler, device, 0u, sampler);
  CHECK_EQ(rig.destroyer.count, 0u);
  CHECK(rig.guard.release(set, device));
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.destroyer.destroyed(sampler)); }

TEST(vulkan_guard, an_acceleration_structure_a_recording_buffer_names_is_destroyed_only_after_the_buffer_finishes)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::acceleration_structure_khr, structure}));
  CHECK(rig.guard.holding(cb, structure));
  CHECK(rig.submit(cb) != 0u);
  rig.guard.destroy(VulkanDestroyKind::acceleration_structure, device, 0u, structure);
  CHECK(rig.guard.release(cb, device));
  CHECK(!rig.destroyer.destroyed(structure));
  rig.guard.reap();
  CHECK(!rig.destroyer.destroyed(structure));
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(structure));
  CHECK(rig.destroyer.kinds[rig.destroyer.order_of(structure)] == VulkanDestroyKind::acceleration_structure); }

TEST(vulkan_guard, a_descriptor_set_written_with_an_acceleration_structure_keeps_it_until_the_set_is_released)
{ Rig rig;
  const VkAccelerationStructureKHR structures[1] = {handle_of<VkAccelerationStructureKHR>(structure)};
  VkWriteDescriptorSetAccelerationStructureKHR chain = {};
  chain.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
  chain.accelerationStructureCount = 1u;
  chain.pAccelerationStructures = structures;
  VkWriteDescriptorSet write = write_of(set, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0u, 0u, 1u);
  write.pNext = &chain;
  CHECK(rig.guard.written(&write, 1u, device));
  CHECK(rig.guard.holding(set, structure));
  rig.guard.destroy(VulkanDestroyKind::acceleration_structure, device, 0u, structure);
  CHECK(!rig.destroyer.destroyed(structure));
  CHECK(rig.guard.release(set, device));
  CHECK(rig.destroyer.destroyed(structure)); }

TEST(vulkan_guard, releasing_a_device_the_guard_never_tracked_still_says_whether_it_was_idle)
{ Rig rig;
  CHECK(rig.guard.release_all(other_device));
  rig.progress.timeout = true;
  CHECK(!rig.guard.release_all(other_device)); }

TEST(vulkan_guard, only_the_handles_a_descriptor_type_reads_are_kept)
{ Rig rig;
  VkDescriptorImageInfo images[1] = {};
  images[0].imageView = handle_of<VkImageView>(view);
  images[0].sampler = handle_of<VkSampler>(sampler);
  VkWriteDescriptorSet sampled = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u);
  sampled.pImageInfo = images;
  VkWriteDescriptorSet sampler_only = write_of(other_set, VK_DESCRIPTOR_TYPE_SAMPLER, 0u, 0u, 1u);
  sampler_only.pImageInfo = images;
  CHECK(rig.guard.written(&sampled, 1u, device));
  CHECK(rig.guard.written(&sampler_only, 1u, device));
  CHECK(rig.guard.holding(set, view));
  CHECK(!rig.guard.holding(set, sampler));
  CHECK(rig.guard.holding(other_set, sampler));
  CHECK(!rig.guard.holding(other_set, view)); }

TEST(vulkan_guard, writing_a_slot_again_gives_the_old_resource_up_and_keeps_one_holding_per_slot)
{ Rig rig;
  VkDescriptorImageInfo old_image[1] = {};
  old_image[0].imageView = handle_of<VkImageView>(view);
  VkDescriptorImageInfo new_image[1] = {};
  new_image[0].imageView = handle_of<VkImageView>(other_view);
  VkWriteDescriptorSet first = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4u, 7u, 1u);
  first.pImageInfo = old_image;
  VkWriteDescriptorSet second = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4u, 7u, 1u);
  second.pImageInfo = new_image;
  CHECK(rig.guard.written(&first, 1u, device));
  rig.guard.destroy(VulkanDestroyKind::image_view, device, 0u, view);
  CHECK(!rig.destroyer.destroyed(view));
  for (std::uint32_t repeat = 0u; repeat < 3u; repeat++) CHECK(rig.guard.written(&second, 1u, device));
  CHECK(rig.destroyer.destroyed(view));
  CHECK(!rig.guard.holding(set, view));
  CHECK(rig.guard.holding(set, other_view));
  CHECK(rig.guard.described(set, 4u, 7u, other_view));
  CHECK(!rig.guard.described(set, 4u, 6u, other_view));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_slot_overwritten_after_the_set_was_submitted_gives_its_old_resource_up_after_the_run)
{ Rig rig;
  VkDescriptorImageInfo old_image[1] = {};
  old_image[0].imageView = handle_of<VkImageView>(view);
  VkDescriptorImageInfo new_image[1] = {};
  new_image[0].imageView = handle_of<VkImageView>(other_view);
  VkWriteDescriptorSet first = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u);
  first.pImageInfo = old_image;
  VkWriteDescriptorSet second = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u);
  second.pImageInfo = new_image;
  CHECK(rig.guard.written(&first, 1u, device));
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.destroy(VulkanDestroyKind::image_view, device, 0u, view);
  CHECK(rig.guard.written(&second, 1u, device));
  CHECK(!rig.destroyer.destroyed(view));
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(view)); }

TEST(vulkan_guard, a_write_to_a_null_set_or_of_nothing_keeps_nothing)
{ Rig rig;
  VkDescriptorImageInfo images[1] = {};
  VkWriteDescriptorSet  writes[3] = {write_of(0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u),
                                     write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u),
                                     write_of(set, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 1u, 0u, 4u)};
  writes[0].pImageInfo = images;
  writes[1].pImageInfo = images;
  CHECK(rig.guard.written(writes, 3u, device));
  CHECK(!rig.guard.tracked(set));
  CHECK(!rig.guard.described(set, 0u, 0u, 0u));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_copied_descriptor_keeps_what_the_slot_it_copied_from_kept_and_clears_what_it_copied_from_nothing)
{ Rig rig;
  VkDescriptorBufferInfo buffers[2] = {};
  buffers[0].buffer = handle_of<VkBuffer>(buffer);
  buffers[1].buffer = handle_of<VkBuffer>(image);
  VkWriteDescriptorSet write = write_of(set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, 0u, 2u);
  write.pBufferInfo = buffers;
  CHECK(rig.guard.written(&write, 1u, device));
  VkDescriptorBufferInfo stale[1] = {};
  stale[0].buffer = handle_of<VkBuffer>(memory);
  VkWriteDescriptorSet stale_write = write_of(other_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5u, 2u, 1u);
  stale_write.pBufferInfo = stale;
  CHECK(rig.guard.written(&stale_write, 1u, device));
  VkCopyDescriptorSet copy = {};
  copy.srcSet = handle_of<VkDescriptorSet>(set);
  copy.srcBinding = 1u;
  copy.dstSet = handle_of<VkDescriptorSet>(other_set);
  copy.dstBinding = 5u;
  copy.dstArrayElement = 1u;
  copy.descriptorCount = 2u;
  CHECK(rig.guard.copied(&copy, 1u, device));
  CHECK(rig.guard.described(other_set, 5u, 1u, buffer));
  CHECK(rig.guard.described(other_set, 5u, 2u, image));
  CHECK(!rig.guard.described(other_set, 5u, 0u, buffer));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(rig.guard.release(set, device));
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK(rig.guard.release(other_set, device));
  CHECK(rig.destroyer.destroyed(buffer)); }

TEST(vulkan_guard, a_slot_the_guard_cannot_keep_is_refused_and_keeps_no_reference)
{ Rig rig;
  VkDescriptorBufferInfo buffers[1] = {};
  buffers[0].buffer = handle_of<VkBuffer>(buffer);
  VkWriteDescriptorSet write = write_of(set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u, 1u);
  write.pBufferInfo = buffers;
  CHECK(rig.guard.bound(set, 0u, 0u, 0u, 0u, device));
  rig.platform.fail_allocation_at(1u);
  CHECK(!rig.guard.written(&write, 1u, device));
  CHECK(rig.guard.invariants_hold()); }

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

TEST(vulkan_guard, a_secondary_freed_while_a_primary_still_names_it_is_not_freed_alone_once_its_pool_is_destroyed)
{ Rig rig;
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, command_pool, device));
  CHECK(rig.guard.reference(HandleKind::command_buffer, secondary, command_pool, 0u));
  rig.guard.begin_frame(false);
  rig.free_command_buffer(secondary, command_pool);
  CHECK(!rig.destroyer.destroyed(secondary));
  rig.destroy_pool(VulkanDestroyKind::command_pool, command_pool, secondary, 0u);
  CHECK(rig.destroyer.destroyed(command_pool));
  rig.free_command_buffer(cb, command_pool);
  CHECK(!rig.destroyer.destroyed(secondary));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_secondary_the_primary_names_survives_a_begin_of_its_own_and_is_left_to_its_pool_s_destroy)
{ Rig rig;
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, command_pool, device));
  CHECK(rig.guard.reference(HandleKind::command_buffer, secondary, command_pool, 0u));
  rig.guard.begin_frame(false);
  CHECK(rig.begin(secondary));
  CHECK(rig.guard.linked(secondary, command_pool));
  rig.free_command_buffer(secondary, command_pool);
  rig.destroy_pool(VulkanDestroyKind::command_pool, command_pool, 0u, 0u);
  CHECK(rig.destroyer.destroyed(command_pool));
  CHECK(rig.begin(cb));
  CHECK(!rig.destroyer.destroyed(secondary));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_pool_destroyed_while_a_command_buffer_names_a_set_of_it_goes_at_once_and_frees_the_set_with_it)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  rig.free_descriptor_set(set, pool);
  CHECK(!rig.destroyer.destroyed(set));
  rig.destroy_pool(VulkanDestroyKind::descriptor_pool, pool, set, 0u);
  CHECK(rig.destroyer.destroyed(pool));
  CHECK(rig.begin(cb));
  CHECK(!rig.destroyer.destroyed(set));
  CHECK_EQ(rig.destroyer.count, 1u);
  CHECK(!rig.guard.tracked(pool));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_pool_reset_drops_the_frees_that_were_waiting_so_a_reused_set_handle_is_not_freed_by_them)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  rig.free_descriptor_set(set, pool);
  CHECK(rig.reset_descriptor_pool(pool, set, 0u));
  CHECK(!rig.guard.linked(set, pool));
  CHECK(!rig.guard.tracked(set));
  CHECK(rig.record(other_cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK(rig.guard.tracked(set));
  CHECK(rig.guard.linked(set, pool));
  CHECK(rig.begin(cb));
  CHECK(!rig.destroyer.destroyed(set));
  CHECK(rig.guard.holding(other_cb, set));
  CHECK(rig.begin(other_cb));
  rig.free_descriptor_set(set, pool);
  CHECK(rig.destroyer.destroyed(set));
  CHECK_EQ(rig.destroyer.count, 1u);
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_pool_reset_with_a_set_still_in_a_primary_leaves_the_primary_to_let_it_go_without_freeing_it)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  VkDescriptorBufferInfo buffers[1] = {};
  buffers[0].buffer = handle_of<VkBuffer>(buffer);
  VkWriteDescriptorSet write = write_of(set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u, 1u);
  write.pBufferInfo = buffers;
  CHECK(rig.guard.written(&write, 1u, device));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(rig.reset_descriptor_pool(pool, set, 0u));
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.begin(cb));
  CHECK(!rig.destroyer.destroyed(set));
  CHECK(!rig.guard.tracked(set));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_reset_that_the_gpu_does_not_finish_in_time_is_refused_and_touches_nothing)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.progress.timeout = true;
  CHECK(!rig.reset_descriptor_pool(pool, set, 0u));
  CHECK(rig.guard.holding(cb, set));
  CHECK(rig.guard.tracked(set)); }

TEST(vulkan_guard, a_reset_waits_for_the_gpu_only_when_the_command_buffer_may_still_run)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK(rig.guard.await_reset(cb, device));
  CHECK_EQ(rig.progress.waits, 0u);
  CHECK_EQ(rig.submit(cb), 1u);
  CHECK(rig.guard.await_reset(cb, device));
  CHECK_EQ(rig.progress.waits, 1u);
  CHECK(rig.guard.await_reset(cb, device));
  CHECK_EQ(rig.progress.waits, 1u); }

TEST(vulkan_guard, a_reset_the_gpu_cannot_be_waited_for_is_refused)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.progress.timeout = true;
  CHECK(!rig.guard.await_reset(cb, device));
  CHECK(!rig.begin(cb));
  CHECK(rig.guard.holding(cb, buffer));
  CHECK(!rig.guard.quiesce(device)); }

TEST(vulkan_guard, letting_everything_go_waits_for_the_gpu_and_finishes_the_destroys_the_guest_asked_for)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.progress.marked_ = 3u;
  static_cast<void>(rig.guard.release_all(device));
  CHECK_EQ(rig.progress.waits, 1u);
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(!rig.guard.tracked(buffer));
  CHECK_EQ(rig.guard.devices(), 0u);
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, letting_a_device_go_whose_work_never_finished_leaks_its_objects_instead_of_destroying_them)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.progress.timeout = true;
  static_cast<void>(rig.guard.release_all(device));
  CHECK_EQ(rig.destroyer.count, 0u);
  CHECK_EQ(rig.guard.devices(), 0u); }

TEST(vulkan_guard, letting_a_device_go_finishes_a_view_destroyed_in_use_and_the_image_it_names_without_leaving_retired_work)
{ Rig rig;
  CHECK(rig.guard.depends(view, image, device));
  CHECK(rig.record(cb, Named{HandleKind::image_view, view}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.destroy(VulkanDestroyKind::image_view, device, 0u, view);
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  CHECK(!rig.destroyer.destroyed(view));
  static_cast<void>(rig.guard.release_all(device));
  CHECK(rig.destroyer.destroyed(view));
  CHECK(rig.destroyer.destroyed(image));
  rig.guard.reap();
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, letting_one_device_go_leaves_another_device_s_holds_and_pending_destroys_alone)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}, Named{HandleKind::device, 0u}, device));
  CHECK(rig.record(other_cb, Named{HandleKind::image, image}, Named{HandleKind::device, 0u}, other_device));
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.guard.destroy(VulkanDestroyKind::image, other_device, 0u, image);
  rig.progress.marked_ = 2u;
  static_cast<void>(rig.guard.release_all(device));
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(!rig.destroyer.destroyed(image));
  CHECK(rig.guard.holding(other_cb, image));
  CHECK_EQ(rig.guard.devices(), 1u);
  CHECK(rig.guard.release(other_cb, other_device));
  CHECK(rig.destroyer.destroyed(image)); }

TEST(vulkan_guard, a_hold_that_cannot_be_kept_refuses_the_command)
{ Rig rig;
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, 0u, device));
  rig.platform.fail_allocation_at(1u);
  CHECK(!rig.guard.reference(HandleKind::buffer, buffer, 0u, 0u));
  CHECK(!rig.guard.tracked(buffer)); }

TEST(vulkan_guard, a_destroy_that_cannot_be_deferred_waits_for_the_gpu_then_goes_through)
{ Rig rig;
  rig.progress.marked_ = 5u;
  rig.progress.reach(device, 1u);
  rig.platform.fail_allocation_at(1u);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK_EQ(rig.progress.waits, 1u);
  CHECK(rig.destroyer.destroyed(buffer)); }

TEST(vulkan_guard, a_destroy_that_cannot_be_deferred_and_cannot_wait_for_the_gpu_leaks_the_object)
{ Rig rig;
  rig.progress.marked_ = 5u;
  rig.progress.reach(device, 1u);
  rig.progress.timeout = true;
  rig.platform.fail_allocation_at(1u);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK_EQ(rig.progress.waits, 1u);
  CHECK_EQ(rig.destroyer.count, 0u); }

TEST(vulkan_guard, a_release_that_cannot_be_deferred_waits_then_retries_and_fails_when_the_wait_does)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.platform.fail_allocation_at(1u);
  CHECK(rig.guard.release(cb, device));
  CHECK_EQ(rig.progress.waits, 1u);
  CHECK(rig.record(other_cb, Named{HandleKind::buffer, buffer}));
  CHECK_EQ(rig.submit(other_cb), 2u);
  rig.platform.fail_allocation_at(1u);
  rig.progress.timeout = true;
  CHECK(!rig.guard.release(other_cb, device));
  CHECK(rig.guard.holding(other_cb, buffer)); }

TEST(vulkan_guard, a_slot_overwrite_that_cannot_be_deferred_is_refused_without_forgetting_the_old_resource)
{ Rig rig;
  VkDescriptorImageInfo old_image[1] = {};
  old_image[0].imageView = handle_of<VkImageView>(view);
  VkWriteDescriptorSet first = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u);
  first.pImageInfo = old_image;
  CHECK(rig.guard.written(&first, 1u, device));
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK_EQ(rig.submit(cb), 1u);
  VkDescriptorImageInfo new_image[1] = {};
  new_image[0].imageView = handle_of<VkImageView>(other_view);
  VkWriteDescriptorSet second = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u);
  second.pImageInfo = new_image;
  rig.platform.fail_allocation_at(1u);
  CHECK(!rig.guard.written(&second, 1u, device));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, imported_memory_freed_while_a_buffer_bound_to_it_is_still_in_use_is_released_only_after_the_buffer)
{ Rig rig;
  CHECK(rig.guard.depends(buffer, memory, device));
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.destroy(VulkanDestroyKind::imported_memory, device, 0u, memory);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  CHECK(!rig.destroyer.destroyed(memory));
  CHECK(rig.guard.release(cb, device));
  CHECK(!rig.destroyer.destroyed(memory));
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.destroyer.destroyed(memory));
  CHECK(rig.destroyer.order_of(buffer) < rig.destroyer.order_of(memory));
  CHECK(rig.destroyer.kinds[rig.destroyer.order_of(memory)] == VulkanDestroyKind::imported_memory);
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, an_imported_semaphore_a_submission_may_wait_on_is_destroyed_only_after_the_gpu_is_past_it)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}, Named{HandleKind::device, 0u}, device));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.destroy(VulkanDestroyKind::imported_semaphore, device, 0u, memory);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK_EQ(rig.destroyer.count, 1u);
  CHECK(rig.destroyer.kinds[0] == VulkanDestroyKind::imported_semaphore); }

TEST(vulkan_guard, imported_memory_nothing_uses_is_released_at_once_and_one_that_cannot_wait_is_leaked)
{ Rig rig;
  rig.guard.destroy(VulkanDestroyKind::imported_memory, device, 0u, memory);
  CHECK(rig.destroyer.destroyed(memory));
  rig.progress.marked_ = 5u;
  rig.progress.reach(device, 1u);
  rig.progress.timeout = true;
  rig.platform.fail_allocation_at(1u);
  rig.guard.destroy(VulkanDestroyKind::imported_memory, device, 0u, other_view);
  CHECK(!rig.destroyer.destroyed(other_view)); }

TEST(vulkan_guard, a_device_whose_work_is_behind_keeps_its_destroys_waiting_while_another_device_s_reaps_go_through)
{ Rig rig;
  rig.progress.marked_ = 6u;
  rig.progress.reach(device, 2u);
  rig.progress.reach(other_device, 2u);
  rig.guard.destroy(VulkanDestroyKind::buffer, device, 0u, buffer);
  rig.guard.destroy(VulkanDestroyKind::image, other_device, 0u, image);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.progress.reach(device, 6u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(!rig.destroyer.destroyed(image));
  rig.progress.reach(other_device, 6u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(image)); }

TEST(vulkan_guard, a_pool_destroyed_while_a_set_of_it_is_still_tracked_is_destroyed_with_the_device_instead_of_being_dropped)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK(rig.begin(cb));
  CHECK(rig.guard.tracked(set));
  rig.guard.destroy(VulkanDestroyKind::descriptor_pool, device, 0u, pool);
  CHECK(!rig.destroyer.destroyed(pool));
  static_cast<void>(rig.guard.release_all(device));
  CHECK(rig.destroyer.destroyed(pool));
  CHECK(!rig.destroyer.destroyed(set));
  CHECK_EQ(rig.guard.devices(), 0u); }

TEST(vulkan_guard, letting_a_device_go_destroys_the_freed_children_of_a_destroyed_pool_before_the_pool)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::command_buffer, secondary, command_pool}));
  CHECK(rig.begin(cb));
  rig.guard.destroy(VulkanDestroyKind::command_buffer, device, command_pool, secondary);
  rig.guard.destroy(VulkanDestroyKind::command_pool, device, 0u, command_pool);
  static_cast<void>(rig.guard.release_all(device));
  CHECK(rig.destroyer.destroyed(secondary));
  CHECK(rig.destroyer.destroyed(command_pool));
  CHECK(rig.destroyer.order_of(secondary) < rig.destroyer.order_of(command_pool)); }

TEST(vulkan_guard, a_device_whose_work_never_finished_leaves_a_destroyed_pool_alone_too)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK(rig.begin(cb));
  rig.guard.destroy(VulkanDestroyKind::descriptor_pool, device, 0u, pool);
  rig.progress.timeout = true;
  static_cast<void>(rig.guard.release_all(device));
  CHECK_EQ(rig.destroyer.count, 0u); }

TEST(vulkan_guard, an_image_used_through_a_set_after_a_submission_is_freed_only_after_that_submission)
{ Rig rig;
  CHECK(rig.guard.depends(view, image, device));
  VkDescriptorImageInfo images[1] = {};
  images[0].imageView = handle_of<VkImageView>(view);
  VkWriteDescriptorSet write = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u);
  write.pImageInfo = images;
  CHECK(rig.guard.written(&write, 1u, device));
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.destroy(VulkanDestroyKind::image_view, device, 0u, view);
  rig.guard.destroy(VulkanDestroyKind::image, device, 0u, image);
  CHECK(rig.guard.release(set, device));
  CHECK(rig.guard.release(cb, device));
  CHECK(!rig.destroyer.destroyed(view));
  CHECK(!rig.destroyer.destroyed(image));
  rig.guard.reap();
  CHECK(!rig.destroyer.destroyed(image));
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK(rig.destroyer.destroyed(view));
  CHECK(rig.destroyer.destroyed(image));
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_submission_marks_the_sets_it_names_and_the_secondaries_they_run_but_not_what_the_sets_hold)
{ Rig rig;
  CHECK(rig.guard.depends(view, image, device));
  VkDescriptorImageInfo images[1] = {};
  images[0].imageView = handle_of<VkImageView>(view);
  VkWriteDescriptorSet write = write_of(set, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u, 1u);
  write.pImageInfo = images;
  CHECK(rig.guard.written(&write, 1u, device));
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::command_buffer, cb, command_pool, device));
  CHECK(rig.guard.reference(HandleKind::command_buffer, secondary, command_pool, 0u));
  rig.guard.begin_frame(true);
  CHECK(rig.guard.reference(HandleKind::command_buffer, secondary, command_pool, device));
  CHECK(rig.guard.reference(HandleKind::descriptor_set, set, pool, 0u));
  rig.guard.begin_frame(false);
  CHECK_EQ(rig.submit(cb), 1u);
  CHECK_EQ(rig.guard.mark_of(cb, device), 1u);
  CHECK_EQ(rig.guard.mark_of(secondary, device), 1u);
  CHECK_EQ(rig.guard.mark_of(set, device), 1u);
  CHECK_EQ(rig.guard.mark_of(view, device), 0u);
  CHECK_EQ(rig.guard.mark_of(image, device), 0u); }

TEST(vulkan_guard, a_reset_waits_only_through_the_mark_of_the_command_buffer_it_resets)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}));
  CHECK(rig.record(other_cb, Named{HandleKind::image, image}));
  CHECK_EQ(rig.submit(cb), 1u);
  CHECK_EQ(rig.submit(other_cb), 2u);
  CHECK(rig.guard.await_reset(cb, device));
  CHECK_EQ(rig.progress.waited_through, 1u);
  CHECK_EQ(rig.progress.reached(device), 1u);
  CHECK(rig.guard.await_reset(other_cb, device));
  CHECK_EQ(rig.progress.waited_through, 2u); }

TEST(vulkan_guard, waiting_for_a_mark_the_gpu_is_already_past_waits_for_nothing)
{ Rig rig;
  CHECK_EQ(rig.submit(cb), 1u);
  rig.progress.reach(device, 1u);
  const std::uint64_t past[2] = {1u, 0u};
  CHECK(rig.guard.quiesce_marks(past, 2u, device));
  CHECK(rig.guard.quiesce_marks(nullptr, 0u, device));
  CHECK_EQ(rig.progress.waits, 0u);
  const std::uint64_t ahead[1] = {2u};
  CHECK(rig.guard.quiesce_marks(ahead, 1u, device));
  CHECK_EQ(rig.progress.waited_through, 2u);
  rig.progress.timeout = true;
  const std::uint64_t later[1] = {3u};
  CHECK(!rig.guard.quiesce_marks(later, 1u, device)); }

TEST(vulkan_guard, a_pool_reset_the_gpu_does_not_finish_is_done_after_the_mark_and_a_destroy_before_then_wins)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.defer_pool_reset(pool, device, 1u);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.guard.orphan_children(pool, device);
  rig.guard.reap();
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK_EQ(rig.destroyer.count, 1u);
  CHECK(rig.destroyer.kinds[0] == VulkanDestroyKind::descriptor_pool_reset);
  CHECK_EQ(rig.destroyer.handles[0], pool);
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, a_pool_destroyed_while_its_reset_waits_is_destroyed_once_and_not_reset)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::descriptor_set, set, pool}));
  CHECK_EQ(rig.submit(cb), 1u);
  rig.guard.defer_pool_reset(pool, device, 1u);
  rig.guard.orphan_children(pool, device);
  rig.guard.destroy(VulkanDestroyKind::descriptor_pool, device, 0u, pool);
  rig.progress.reach(device, 1u);
  rig.guard.reap();
  CHECK_EQ(rig.destroyer.count, 1u);
  CHECK(rig.destroyer.kinds[0] == VulkanDestroyKind::descriptor_pool);
  CHECK(rig.guard.invariants_hold()); }

TEST(vulkan_guard, draining_waits_for_every_device_s_work_and_leaves_the_objects_alone)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}, Named{HandleKind::device, 0u}, device));
  CHECK(rig.record(other_cb, Named{HandleKind::image, image}, Named{HandleKind::device, 0u}, other_device));
  CHECK_EQ(rig.submit(cb, device), 1u);
  CHECK_EQ(rig.submit(other_cb, other_device), 2u);
  av::GenerousBudget budget;
  CHECK(rig.guard.drain(budget.budget()));
  CHECK_EQ(rig.progress.waits, 2u);
  CHECK_EQ(rig.progress.reached(device), 2u);
  CHECK_EQ(rig.progress.reached(other_device), 2u);
  CHECK(rig.guard.holding(cb, buffer));
  CHECK_EQ(rig.destroyer.count, 0u); }

TEST(vulkan_guard, a_drain_the_gpu_does_not_finish_in_time_reports_it_and_still_asks_every_device)
{ Rig rig;
  CHECK(rig.record(cb, Named{HandleKind::buffer, buffer}, Named{HandleKind::device, 0u}, device));
  CHECK(rig.record(other_cb, Named{HandleKind::image, image}, Named{HandleKind::device, 0u}, other_device));
  CHECK_EQ(rig.submit(cb, device), 1u);
  CHECK_EQ(rig.submit(other_cb, other_device), 2u);
  rig.progress.timeout = true;
  av::GenerousBudget budget;
  CHECK(!rig.guard.drain(budget.budget()));
  CHECK_EQ(rig.progress.waits, 2u);
  CHECK_EQ(rig.destroyer.count, 0u); }

TEST(vulkan_guard, a_wait_for_several_marks_asks_for_exactly_those_marks_and_ones_the_gpu_is_past_ask_for_nothing)
{ Rig rig;
  CHECK_EQ(rig.submit(cb), 1u);
  CHECK_EQ(rig.submit(other_cb), 2u);
  CHECK_EQ(rig.submit(cb), 3u);
  const std::uint64_t some[2] = {1u, 3u};
  CHECK(rig.guard.quiesce_marks(some, 2u, device));
  CHECK_EQ(rig.progress.waited_marks, 2u);
  CHECK_EQ(rig.progress.waits, 1u); }
