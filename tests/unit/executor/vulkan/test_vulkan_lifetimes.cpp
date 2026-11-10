#include "executor/vulkan/vulkan_destroyer.hpp"
#include "executor/vulkan/vulkan_lifetimes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::VulkanDestroyKind;
using av::VulkanLifetimes;

namespace {

constexpr std::uint64_t device = 1u;
constexpr std::uint64_t list = 100u;
constexpr std::uint64_t other_list = 101u;
constexpr std::uint64_t buffer = 200u;
constexpr std::uint64_t image = 201u;
constexpr std::uint64_t view = 202u;
constexpr std::uint64_t set = 203u;
constexpr std::uint64_t pool = 204u;
constexpr std::uint64_t memory = 205u;

class RecordingDestroyer final : public av::VulkanDestroyer {
public:
    void destroy(VulkanDestroyKind kind, std::uint64_t, std::uint64_t parent_of, std::uint64_t handle) override
    {
        if (count < 16u) {
            handles[count] = handle;
            kinds[count] = kind;
            parents[count] = parent_of;
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
    std::uint64_t     parents[16] = {};
    VulkanDestroyKind kinds[16] = {};
    std::uint32_t     count = 0u;
};

struct Rig {
    av::MockPlatform platform;
    RecordingDestroyer destroyer;
    VulkanLifetimes    lifetimes{platform, destroyer};

    av::Status destroy(VulkanDestroyKind kind, std::uint64_t handle, std::uint64_t marked, std::uint64_t reached,
                       std::uint64_t parent = 0u)
    {
        return lifetimes.destroy(kind, device, parent, handle, marked, reached);
    }
    av::Status hold(std::uint64_t owner, std::uint64_t handle, std::uint64_t parent = 0u)
    {
        return lifetimes.hold(owner, handle, device, parent);
    }
};

}

TEST(vulkan_lifetimes, an_object_nothing_holds_is_destroyed_at_once_when_the_gpu_is_idle)
{ Rig rig;
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 4u, 4u), av::Status::ok);
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.destroyer.kinds[0] == VulkanDestroyKind::buffer);
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, an_object_destroyed_while_the_gpu_works_is_destroyed_when_the_gpu_is_past_the_work)
{ Rig rig;
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image, image, 6u, 3u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(image));
  CHECK_EQ(rig.lifetimes.retired(), 1u);
  rig.lifetimes.reap(5u);
  CHECK(!rig.destroyer.destroyed(image));
  rig.lifetimes.reap(6u);
  CHECK(rig.destroyer.destroyed(image));
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_buffer_destroyed_after_a_command_buffer_recorded_it_lives_until_that_command_buffer_is_reset)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  CHECK(rig.lifetimes.holding(list, buffer));
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 0u, 0u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK(rig.lifetimes.tracked(buffer));
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(!rig.lifetimes.tracked(buffer));
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, an_object_a_submitted_command_buffer_holds_outlives_the_command_buffer_s_reset_until_the_gpu_is_done)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  rig.lifetimes.submitted(list, 5u);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 5u, 2u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(list, 2u), av::Status::ok);
  rig.lifetimes.reap(4u);
  CHECK(!rig.destroyer.destroyed(buffer));
  rig.lifetimes.reap(5u);
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_buffer_destroyed_after_the_command_buffer_that_recorded_it_finished_running_is_destroyed_at_once)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  rig.lifetimes.submitted(list, 3u);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 3u, 3u), av::Status::ok);
  CHECK(rig.destroyer.destroyed(buffer));
  CHECK(!rig.lifetimes.tracked(buffer));
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_command_buffer_run_again_is_kept_to_its_latest_submission)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  rig.lifetimes.submitted(list, 3u);
  rig.lifetimes.submitted(list, 8u);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 8u, 0u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(list, 7u), av::Status::ok);
  rig.lifetimes.reap(7u);
  CHECK(!rig.destroyer.destroyed(buffer));
  rig.lifetimes.reap(8u);
  CHECK(rig.destroyer.destroyed(buffer)); }

TEST(vulkan_lifetimes, two_command_buffers_each_keep_the_object_alive)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  CHECK_STATUS(rig.hold(other_list, buffer), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 0u, 0u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK_STATUS(rig.lifetimes.release(other_list, 0u), av::Status::ok);
  CHECK(rig.destroyer.destroyed(buffer)); }

TEST(vulkan_lifetimes, a_view_keeps_the_image_it_names_alive_and_the_image_is_destroyed_after_the_view)
{ Rig rig;
  CHECK_STATUS(rig.hold(view, image), av::Status::ok);
  CHECK_STATUS(rig.hold(list, view), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image, image, 0u, 0u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image_view, view, 0u, 0u), av::Status::ok);
  CHECK_EQ(rig.destroyer.count, 0u);
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK_EQ(rig.destroyer.count, 2u);
  CHECK_EQ(rig.destroyer.handles[0], view);
  CHECK_EQ(rig.destroyer.handles[1], image);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, memory_bound_to_a_buffer_is_freed_after_the_buffer)
{ Rig rig;
  CHECK_STATUS(rig.hold(buffer, memory), av::Status::ok);
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::memory, memory, 0u, 0u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 0u, 0u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(memory));
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK(rig.destroyer.kinds[0] == VulkanDestroyKind::buffer);
  CHECK(rig.destroyer.kinds[1] == VulkanDestroyKind::memory); }

TEST(vulkan_lifetimes, a_submission_marks_what_the_command_buffer_reaches_through_sets_and_views)
{ Rig rig;
  CHECK_STATUS(rig.hold(view, image), av::Status::ok);
  CHECK_STATUS(rig.hold(set, view), av::Status::ok);
  CHECK_STATUS(rig.hold(list, set), av::Status::ok);
  rig.lifetimes.submitted(list, 9u);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image_view, view, 9u, 1u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image, image, 9u, 1u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(set, 1u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(list, 1u), av::Status::ok);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.lifetimes.reap(8u);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.lifetimes.reap(9u);
  CHECK(rig.destroyer.destroyed(view));
  CHECK(rig.destroyer.destroyed(image)); }

TEST(vulkan_lifetimes, a_set_or_command_buffer_keeps_the_pool_it_came_from_alive)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, set, pool), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_pool, pool, 0u, 0u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(pool));
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_set, set, 0u, 0u, pool), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(set));
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK(rig.destroyer.kinds[0] == VulkanDestroyKind::descriptor_set);
  CHECK_EQ(rig.destroyer.parents[0], pool);
  CHECK(rig.destroyer.kinds[1] == VulkanDestroyKind::descriptor_pool);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, what_cannot_be_recorded_is_refused_and_nothing_is_destroyed_early)
{ Rig rig;
  CHECK_STATUS(rig.lifetimes.hold(0u, buffer, device, 0u), av::Status::invalid_parameter);
  CHECK_STATUS(rig.lifetimes.hold(list, 0u, device, 0u), av::Status::invalid_parameter);
  rig.platform.fail_allocation_at(1u);
  CHECK_STATUS(rig.hold(list, buffer), av::Status::no_memory);
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  rig.platform.fail_allocation_at(1u);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 3u, 0u), av::Status::no_memory);
  CHECK_EQ(rig.destroyer.count, 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, letting_everything_go_finishes_the_destroys_the_guest_asked_for_and_forgets_the_rest)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image, image, 4u, 0u), av::Status::ok);
  rig.lifetimes.release_all(true);
  CHECK(rig.destroyer.destroyed(image));
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  CHECK_EQ(rig.lifetimes.retired(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_set_updated_after_a_command_buffer_that_binds_it_was_submitted_gives_the_old_view_up_after_the_run)
{ Rig rig;
  CHECK_STATUS(rig.lifetimes.write(set, av::VulkanLifetimes::Descriptor{0u, 0u}, view, 0u, device, 0u), av::Status::ok);
  CHECK_STATUS(rig.hold(list, set), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image_view, view, 0u, 0u), av::Status::ok);
  rig.lifetimes.submitted(list, 9u);
  CHECK_STATUS(rig.lifetimes.release(set, 1u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(view));
  rig.lifetimes.reap(8u);
  CHECK(!rig.destroyer.destroyed(view));
  rig.lifetimes.reap(9u);
  CHECK(rig.destroyer.destroyed(view)); }

TEST(vulkan_lifetimes, letting_everything_go_after_work_that_defers_more_leaves_no_retired_entry_behind)
{ Rig rig;
  CHECK_STATUS(rig.hold(view, image), av::Status::ok);
  CHECK_STATUS(rig.hold(list, view), av::Status::ok);
  rig.lifetimes.submitted(list, 9u);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image_view, view, 9u, 0u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image, image, 9u, 0u), av::Status::ok);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.lifetimes.release_all(true);
  CHECK(rig.destroyer.destroyed(view));
  CHECK(rig.destroyer.destroyed(image));
  CHECK_EQ(rig.destroyer.count, 2u);
  CHECK_EQ(rig.lifetimes.retired(), 0u);
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  rig.lifetimes.reap(100u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, letting_everything_go_while_the_gpu_may_still_work_destroys_nothing)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, buffer), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image, image, 4u, 0u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::buffer, buffer, 4u, 0u), av::Status::ok);
  rig.lifetimes.release_all(false);
  CHECK_EQ(rig.destroyer.count, 0u);
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  CHECK_EQ(rig.lifetimes.retired(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_child_orphaned_by_a_pool_reset_is_never_freed_and_lets_go_of_its_pool)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, set, pool), av::Status::ok);
  CHECK(rig.lifetimes.linked(set, pool));
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_set, set, 0u, 0u, pool), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_pool, pool, 0u, 0u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(pool));
  rig.lifetimes.orphan_children(pool);
  CHECK(!rig.lifetimes.tracked(set));
  CHECK(rig.destroyer.destroyed(pool));
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK(!rig.destroyer.destroyed(set));
  CHECK_EQ(rig.destroyer.count, 1u);
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, an_alive_child_orphaned_by_a_pool_reset_goes_when_its_last_hold_does)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, set, pool), av::Status::ok);
  rig.lifetimes.orphan_children(pool);
  CHECK(!rig.lifetimes.tracked(set));
  CHECK_STATUS(rig.hold(other_list, set, pool), av::Status::ok);
  CHECK(rig.lifetimes.tracked(set));
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK(rig.lifetimes.holding(other_list, set));
  CHECK_EQ(rig.destroyer.count, 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_descriptor_slot_holds_one_resource_pair_and_a_rewrite_gives_the_old_pair_up_after_the_set_s_last_run)
{ Rig rig;
  using Descriptor = av::VulkanLifetimes::Descriptor;
  CHECK_STATUS(rig.lifetimes.write(set, Descriptor{2u, 3u}, view, image, device, 0u), av::Status::ok);
  CHECK_EQ(rig.lifetimes.descriptors(), 1u);
  CHECK(rig.lifetimes.described(set, Descriptor{2u, 3u}, view));
  CHECK(rig.lifetimes.described(set, Descriptor{2u, 3u}, image));
  CHECK(!rig.lifetimes.described(set, Descriptor{3u, 2u}, view));
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image_view, view, 0u, 0u), av::Status::ok);
  rig.lifetimes.submitted(set, 5u);
  CHECK_STATUS(rig.lifetimes.write(set, Descriptor{2u, 3u}, buffer, 0u, device, 1u), av::Status::ok);
  CHECK_EQ(rig.lifetimes.descriptors(), 1u);
  CHECK(!rig.destroyer.destroyed(view));
  rig.lifetimes.reap(5u);
  CHECK(rig.destroyer.destroyed(view));
  CHECK_STATUS(rig.lifetimes.write(set, Descriptor{2u, 3u}, 0u, 0u, device, 5u), av::Status::ok);
  CHECK_EQ(rig.lifetimes.descriptors(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_set_s_descriptors_are_marked_by_a_submission_and_given_up_after_it_when_the_set_is_released)
{ Rig rig;
  using Descriptor = av::VulkanLifetimes::Descriptor;
  CHECK_STATUS(rig.lifetimes.write(set, Descriptor{0u, 0u}, view, 0u, device, 0u), av::Status::ok);
  CHECK_STATUS(rig.hold(view, image), av::Status::ok);
  CHECK_STATUS(rig.hold(list, set), av::Status::ok);
  rig.lifetimes.submitted(list, 7u);
  CHECK_EQ(rig.lifetimes.mark_of(set), 7u);
  CHECK_EQ(rig.lifetimes.mark_of(view), 0u);
  CHECK_EQ(rig.lifetimes.mark_of(image), 0u);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::image_view, view, 7u, 0u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(set, 0u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK_EQ(rig.lifetimes.descriptors(), 0u);
  CHECK(!rig.destroyer.destroyed(view));
  rig.lifetimes.reap(7u);
  CHECK(rig.destroyer.destroyed(view)); }

TEST(vulkan_lifetimes, copying_descriptors_slot_by_slot_leaves_the_source_alone_and_clears_the_slots_the_source_lacks)
{ Rig rig;
  using Descriptor = av::VulkanLifetimes::Descriptor;
  CHECK_STATUS(rig.lifetimes.write(set, Descriptor{1u, 0u}, view, 0u, device, 0u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.write(other_list, Descriptor{4u, 1u}, buffer, 0u, device, 0u), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.copy(other_list, Descriptor{4u, 0u}, set, Descriptor{1u, 0u}, 2u, 0u), av::Status::ok);
  CHECK(rig.lifetimes.described(other_list, Descriptor{4u, 0u}, view));
  CHECK(!rig.lifetimes.described(other_list, Descriptor{4u, 1u}, buffer));
  CHECK(rig.lifetimes.described(set, Descriptor{1u, 0u}, view));
  CHECK_EQ(rig.lifetimes.descriptors(), 2u);
  rig.platform.fail_allocation_at(1u);
  CHECK_STATUS(rig.lifetimes.copy(pool, Descriptor{9u, 0u}, set, Descriptor{1u, 0u}, 1u, 0u),
               av::Status::no_memory);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, a_node_a_submission_names_keeps_that_mark_with_no_list_or_table)
{ Rig rig;
  CHECK_STATUS(rig.lifetimes.track(set, device), av::Status::ok);
  CHECK_EQ(rig.lifetimes.mark_of(set), 0u);
  rig.lifetimes.submitted(set, 4u);
  CHECK_EQ(rig.lifetimes.mark_of(set), 4u);
  rig.lifetimes.submitted(set, 2u);
  CHECK_EQ(rig.lifetimes.mark_of(set), 4u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, an_object_destroyed_again_while_the_first_request_waits_goes_when_the_later_request_does)
{ Rig rig;
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_pool_reset, pool, 5u, 0u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_pool, pool, 6u, 0u), av::Status::ok);
  rig.lifetimes.reap(5u);
  CHECK_EQ(rig.destroyer.count, 0u);
  rig.lifetimes.reap(6u);
  CHECK_EQ(rig.destroyer.count, 1u);
  CHECK(rig.destroyer.kinds[0] == VulkanDestroyKind::descriptor_pool);
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }

TEST(vulkan_lifetimes, letting_everything_go_destroys_the_children_asked_for_before_their_pool_and_drops_the_rest)
{ Rig rig;
  CHECK_STATUS(rig.hold(list, set, pool), av::Status::ok);
  CHECK_STATUS(rig.hold(list, view, pool), av::Status::ok);
  CHECK_STATUS(rig.lifetimes.release(list, 0u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_pool, pool, 0u, 0u), av::Status::ok);
  CHECK_STATUS(rig.destroy(VulkanDestroyKind::descriptor_set, set, 0u, 0u, pool), av::Status::ok);
  CHECK_EQ(rig.destroyer.count, 1u);
  rig.lifetimes.release_all(true);
  CHECK(rig.destroyer.destroyed(pool));
  CHECK(!rig.destroyer.destroyed(view));
  CHECK_EQ(rig.lifetimes.nodes(), 0u); }
