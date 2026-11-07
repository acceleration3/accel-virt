#include "executor/vulkan/vulkan_destroyer.hpp"
#include "executor/vulkan/vulkan_lifetimes.hpp"
#include "executor/vulkan/vulkan_node.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::VulkanDestroyKind;
using av::VulkanNode;

namespace {

class CountingDestroyer final : public av::VulkanDestroyer {
public:
    void destroy(VulkanDestroyKind, std::uint64_t, std::uint64_t, std::uint64_t handle) override
    {
        last = handle;
        count++;
    }

    std::uint64_t last = 0u;
    std::uint32_t count = 0u;
};

}

TEST(vulkan_node, a_child_linked_to_its_pool_holds_a_reference_to_it_until_it_is_unlinked)
{ av::MockPlatform platform;
  CountingDestroyer destroyer;
  av::VulkanLifetimes lifetimes(platform, destroyer);
  CHECK_STATUS(lifetimes.hold(1u, 20u, 7u, 30u), av::Status::ok);
  CHECK_STATUS(lifetimes.hold(1u, 21u, 7u, 30u), av::Status::ok);
  CHECK(lifetimes.linked(20u, 30u));
  CHECK(lifetimes.linked(21u, 30u));
  CHECK(!lifetimes.linked(20u, 31u));
  CHECK_EQ(lifetimes.nodes(), 3u);
  CHECK(lifetimes.invariants_hold()); }

TEST(vulkan_node, an_orphan_leaves_its_pool_and_is_not_a_child_of_it_any_more)
{ av::MockPlatform platform;
  CountingDestroyer destroyer;
  av::VulkanLifetimes lifetimes(platform, destroyer);
  CHECK_STATUS(lifetimes.hold(1u, 20u, 7u, 30u), av::Status::ok);
  CHECK_STATUS(lifetimes.hold(1u, 21u, 7u, 30u), av::Status::ok);
  lifetimes.orphan_children(30u);
  CHECK(!lifetimes.linked(20u, 30u));
  CHECK(!lifetimes.tracked(20u));
  CHECK(!lifetimes.tracked(21u));
  CHECK(lifetimes.tracked(30u));
  CHECK(lifetimes.invariants_hold());
  CHECK_STATUS(lifetimes.release(1u, 0u), av::Status::ok);
  CHECK_EQ(destroyer.count, 0u);
  CHECK_EQ(lifetimes.nodes(), 1u); }

TEST(vulkan_node, a_pool_destroyed_while_children_link_to_it_goes_when_the_last_child_does)
{ av::MockPlatform platform;
  CountingDestroyer destroyer;
  av::VulkanLifetimes lifetimes(platform, destroyer);
  CHECK_STATUS(lifetimes.hold(1u, 20u, 7u, 30u), av::Status::ok);
  CHECK_STATUS(lifetimes.hold(2u, 21u, 7u, 30u), av::Status::ok);
  CHECK_STATUS(lifetimes.destroy(VulkanDestroyKind::command_pool, 7u, 0u, 30u, 0u, 0u), av::Status::ok);
  CHECK_EQ(destroyer.count, 0u);
  CHECK_STATUS(lifetimes.destroy(VulkanDestroyKind::command_buffer, 7u, 30u, 20u, 0u, 0u), av::Status::ok);
  CHECK_STATUS(lifetimes.release(1u, 0u), av::Status::ok);
  CHECK_EQ(destroyer.count, 1u);
  CHECK_EQ(destroyer.last, 20u);
  CHECK_STATUS(lifetimes.destroy(VulkanDestroyKind::command_buffer, 7u, 30u, 21u, 0u, 0u), av::Status::ok);
  CHECK_STATUS(lifetimes.release(2u, 0u), av::Status::ok);
  CHECK_EQ(destroyer.count, 3u);
  CHECK_EQ(destroyer.last, 30u);
  CHECK(lifetimes.invariants_hold()); }

TEST(vulkan_node, children_are_listed_newest_first_and_unlinking_one_closes_the_list_over_it)
{ av::MockPlatform platform;
  CountingDestroyer destroyer;
  av::VulkanLifetimes lifetimes(platform, destroyer);
  VulkanNode pool(lifetimes, 30u, 7u, 0u);
  VulkanNode first(lifetimes, 20u, 7u, 30u);
  VulkanNode second(lifetimes, 21u, 7u, 30u);
  VulkanNode third(lifetimes, 22u, 7u, 30u);
  first.link(&pool);
  second.link(&pool);
  third.link(&pool);
  CHECK_EQ(pool.references(), 4u);
  CHECK(pool.first_child() == &third);
  CHECK(third.next_child() == &second);
  CHECK(second.next_child() == &first);
  CHECK(first.next_child() == nullptr);
  CHECK(second.pool() == &pool);
  CHECK(pool.invariants_hold() && first.invariants_hold() && second.invariants_hold() && third.invariants_hold());
  second.unlink();
  CHECK(second.pool() == nullptr);
  CHECK(third.next_child() == &first);
  CHECK_EQ(pool.references(), 3u);
  third.unlink();
  CHECK(pool.first_child() == &first);
  first.unlink();
  CHECK(pool.first_child() == nullptr);
  CHECK_EQ(pool.references(), 1u);
  second.unlink();
  CHECK_EQ(pool.references(), 1u); }

TEST(vulkan_node, an_orphan_remembers_it_and_a_node_that_was_asked_to_be_destroyed_carries_what_to_destroy)
{ av::MockPlatform platform;
  CountingDestroyer destroyer;
  av::VulkanLifetimes lifetimes(platform, destroyer);
  VulkanNode pool(lifetimes, 30u, 7u, 0u);
  VulkanNode child(lifetimes, 20u, 0u, 0u);
  child.link(&pool);
  child.request(VulkanDestroyKind::descriptor_set, 7u, 30u);
  CHECK(child.kind() == VulkanDestroyKind::descriptor_set);
  CHECK_EQ(child.device(), 7u);
  CHECK_EQ(child.parent(), 30u);
  CHECK(!child.orphaned());
  child.AddRef();
  child.orphan();
  CHECK(child.orphaned());
  CHECK(child.kind() == VulkanDestroyKind::none);
  CHECK(child.pool() == nullptr);
  CHECK_EQ(pool.references(), 1u);
  CHECK_EQ(child.references(), 2u); }
