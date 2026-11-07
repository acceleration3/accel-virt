#include "core/platform/block_source.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/vulkan/vulkan_queue_families.hpp"
#include "harness/framework/test_registry.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace {

constexpr std::uint64_t pool = 0x40u;

struct Rig {
    Rig() : families(heap) {}

    av::HostHeap             heap;
    av::VulkanQueueFamilies  families;
};

}

TEST(vulkan_queue_families, a_graphics_or_compute_family_also_does_transfer_work)
{ Rig rig;
  CHECK(rig.families.record(pool, VK_QUEUE_GRAPHICS_BIT) == av::Status::ok);
  CHECK((rig.families.flags_of(pool) & (VK_QUEUE_TRANSFER_BIT)) != 0u);
  CHECK(rig.families.record(pool + 1u, VK_QUEUE_COMPUTE_BIT) == av::Status::ok);
  CHECK((rig.families.flags_of(pool + 1u) & (VK_QUEUE_TRANSFER_BIT)) != 0u);
  CHECK((rig.families.flags_of(pool + 1u) & (VK_QUEUE_GRAPHICS_BIT)) == 0u);
  CHECK(rig.families.invariants_hold()); }

TEST(vulkan_queue_families, a_video_family_does_no_transfer_work_and_does_its_own)
{ Rig rig;
  CHECK(rig.families.record(pool, VK_QUEUE_VIDEO_DECODE_BIT_KHR) == av::Status::ok);
  CHECK((rig.families.flags_of(pool) & (VK_QUEUE_TRANSFER_BIT)) == 0u);
  CHECK((rig.families.flags_of(pool) & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0u);
  CHECK((rig.families.flags_of(pool) & (VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT)) != 0u); }

TEST(vulkan_queue_families, a_pool_never_recorded_has_no_flags_and_a_forgotten_one_loses_them)
{ Rig rig;
  CHECK((rig.families.flags_of(pool) & (VK_QUEUE_TRANSFER_BIT)) == 0u);
  CHECK(rig.families.record(pool, VK_QUEUE_GRAPHICS_BIT) == av::Status::ok);
  CHECK_EQ(rig.families.count(), 1u);
  rig.families.forget(pool);
  CHECK_EQ(rig.families.count(), 0u);
  CHECK((rig.families.flags_of(pool) & (VK_QUEUE_GRAPHICS_BIT)) == 0u);
  CHECK(rig.families.record(pool, VK_QUEUE_GRAPHICS_BIT) == av::Status::ok);
  rig.families.clear();
  CHECK_EQ(rig.families.count(), 0u); }

TEST(vulkan_queue_families, recording_a_pool_again_replaces_its_flags_and_a_null_pool_is_refused)
{ Rig rig;
  CHECK(rig.families.record(pool, VK_QUEUE_GRAPHICS_BIT) == av::Status::ok);
  CHECK(rig.families.record(pool, VK_QUEUE_VIDEO_ENCODE_BIT_KHR) == av::Status::ok);
  CHECK_EQ(rig.families.count(), 1u);
  CHECK((rig.families.flags_of(pool) & (VK_QUEUE_GRAPHICS_BIT)) == 0u);
  CHECK(rig.families.record(0u, VK_QUEUE_GRAPHICS_BIT) == av::Status::invalid_parameter);
  CHECK_EQ(rig.families.flags_of(pool), VK_QUEUE_VIDEO_ENCODE_BIT_KHR); }
