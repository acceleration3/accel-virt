/* Which of one physical device's memory types the guest may map, and the
 * guest's view of the device's memory narrowed to them (docs/host-memory.md):
 * a type the GPU's memory source cannot allocate, export and map is not host
 * visible to the guest. The masks below are shaped like NVIDIA's six types,
 * whose BAR type 5 is host visible to the host and not mappable through a
 * dma-buf, and Intel's seven. */
#include "executor/vulkan/importable_types.hpp"
#include "harness/executor/importable_types_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ImportableTypes;
using av::ImportableTypesProbe;

namespace {

constexpr std::uint64_t intel = 0x1000u;
constexpr std::uint64_t nvidia = 0x2000u;
constexpr VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
constexpr VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
constexpr VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
constexpr VkMemoryPropertyFlags cached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
constexpr VkMemoryPropertyFlags host_flags = visible | coherent | cached;

VkPhysicalDeviceMemoryProperties types(std::uint32_t count, const VkMemoryPropertyFlags *flags)
{ VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = count;
  properties.memoryHeapCount = 2u;
  for (std::uint32_t type = 0u; type < count; type++) {
      properties.memoryTypes[type].propertyFlags = flags[type];
      properties.memoryTypes[type].heapIndex = type % 2u;
  }
  return properties; }

}  // namespace

TEST(importable_types, knows_no_device_until_it_learns_one)
{ ImportableTypes importable;
  CHECK(importable.invariants_hold());
  CHECK(!importable.known_for(intel));
  CHECK(!importable.known_for(0u));
  CHECK(!importable.includes(0u)); }

/* Learning device 0 is forgetting: no device answers to it, and no mask
 * stays behind for one. */
TEST(importable_types, learning_no_device_forgets_the_one_it_knew)
{ ImportableTypes importable;
  importable.learn(intel, 0x7fu);
  CHECK(importable.includes(0u));
  importable.learn(0u, 0x7fu);
  CHECK(importable.invariants_hold());
  CHECK(!importable.known_for(intel));
  CHECK(!importable.known_for(0u));
  CHECK(!importable.includes(0u)); }

TEST(importable_types, learns_the_mask_of_one_physical_device)
{ ImportableTypes importable;
  importable.learn(nvidia, 0x18u);
  CHECK(importable.invariants_hold());
  CHECK(importable.known_for(nvidia));
  CHECK(!importable.known_for(intel));
  CHECK(!importable.includes(2u));
  CHECK(importable.includes(3u));
  CHECK(importable.includes(4u));
  CHECK(!importable.includes(5u));
  importable.learn(intel, 0x7fu);
  CHECK(importable.invariants_hold());
  CHECK(importable.known_for(intel));
  CHECK(!importable.known_for(nvidia));
  CHECK(importable.includes(6u));
  CHECK(!importable.includes(7u)); }

/* A type past every type a device can have is past the mask's width: never
 * included, and never a shift by 32 or more. */
TEST(importable_types, a_type_past_the_mask_s_width_is_never_included)
{ ImportableTypes importable;
  importable.learn(intel, 0xFFFFFFFFu);
  CHECK(importable.includes(31u));
  CHECK(!importable.includes(32u));
  CHECK(!importable.includes(0xFFFFFFFFu)); }

TEST(importable_types, nvidia_s_bar_type_is_no_longer_host_visible)
{ const VkMemoryPropertyFlags flags[] = {local, local, local, visible | cached,
                                         visible | coherent, local | visible | coherent};
  VkPhysicalDeviceMemoryProperties properties = types(6u, flags);
  ImportableTypes importable;
  importable.learn(nvidia, 0x18u);
  importable.restrict(properties);
  CHECK_EQ(properties.memoryTypeCount, 6u);
  CHECK_EQ(properties.memoryHeapCount, 2u);
  CHECK_EQ(properties.memoryTypes[0].propertyFlags, local);
  CHECK_EQ(properties.memoryTypes[3].propertyFlags, visible | cached);
  CHECK_EQ(properties.memoryTypes[4].propertyFlags, visible | coherent);
  CHECK_EQ(properties.memoryTypes[5].propertyFlags, local);
  CHECK_EQ(properties.memoryTypes[5].heapIndex, 1u); }

TEST(importable_types, intel_keeps_every_type_as_the_host_has_it)
{ const VkMemoryPropertyFlags flags[] = {local | host_flags, local | visible, local, local | host_flags,
                                         local | visible | coherent, local | visible | cached, local | host_flags};
  VkPhysicalDeviceMemoryProperties properties = types(7u, flags);
  ImportableTypes importable;
  importable.learn(intel, 0x7fu);
  importable.restrict(properties);
  for (std::uint32_t type = 0u; type < 7u; type++) CHECK_EQ(properties.memoryTypes[type].propertyFlags, flags[type]);
}

/* HOST_COHERENT and HOST_CACHED are only valid on a host-visible type, so they
 * go with it; nothing is added to a type that was never host visible. */
TEST(importable_types, a_type_losing_host_visibility_loses_what_needs_it_and_gains_nothing)
{ const VkMemoryPropertyFlags flags[] = {host_flags | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, local};
  VkPhysicalDeviceMemoryProperties properties = types(2u, flags);
  ImportableTypes importable;
  importable.learn(intel, 0u);
  importable.restrict(properties);
  CHECK_EQ(properties.memoryTypes[0].propertyFlags, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
  CHECK_EQ(properties.memoryTypes[1].propertyFlags, local); }

/* The count is the driver's, and never trusted past the array. */
TEST(importable_types, a_count_past_the_array_is_restricted_only_within_it)
{ VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = 0xFFFFFFFFu;
  for (auto &type : properties.memoryTypes) type.propertyFlags = host_flags;
  ImportableTypes importable;
  importable.learn(intel, 0x1u);
  importable.restrict(properties);
  CHECK_EQ(properties.memoryTypeCount, 0xFFFFFFFFu);
  CHECK_EQ(properties.memoryTypes[0].propertyFlags, host_flags);
  CHECK_EQ(properties.memoryTypes[VK_MAX_MEMORY_TYPES - 1u].propertyFlags, 0u); }

/* Before any device is learnt nothing is known importable. */
TEST(importable_types, restricting_before_learning_hides_every_host_visible_type)
{ const VkMemoryPropertyFlags flags[] = {host_flags};
  VkPhysicalDeviceMemoryProperties properties = types(1u, flags);
  ImportableTypes importable;
  importable.restrict(properties);
  CHECK_EQ(properties.memoryTypes[0].propertyFlags, 0u); }

TEST(importable_types, invariants_refuse_a_mask_with_no_device)
{ ImportableTypes importable;
  ImportableTypesProbe(importable).set_mask(0x1u);
  CHECK(!importable.invariants_hold()); }
