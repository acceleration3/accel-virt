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
  rig.lifetimes.release_all();
  CHECK(rig.destroyer.destroyed(image));
  CHECK(!rig.destroyer.destroyed(buffer));
  CHECK_EQ(rig.lifetimes.nodes(), 0u);
  CHECK_EQ(rig.lifetimes.retired(), 0u);
  CHECK(rig.lifetimes.invariants_hold()); }
