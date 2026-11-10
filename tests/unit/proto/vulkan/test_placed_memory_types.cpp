#include "harness/framework/test_registry.hpp"
#include "proto/vulkan/placed_memory_types.hpp"

#include <cstdint>

using av::PlacedMemoryTypes;

namespace {

constexpr VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
constexpr VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

VkPhysicalDeviceMemoryProperties made(const VkMemoryType *types, std::uint32_t count)
{ VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = count;
  for (std::uint32_t index = 0u; index < count; index++) properties.memoryTypes[index] = types[index];
  properties.memoryHeapCount = 2u;
  return properties; }

}

TEST(placed_memory_types, no_type_is_placed_without_a_twin)
{ const VkMemoryType types[] = {{local, 0u}, {visible, 1u}, {visible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 1u}};
  CHECK_EQ(PlacedMemoryTypes::mask(made(types, 3u)), 0u); }

TEST(placed_memory_types, both_members_of_a_host_visible_pair_in_one_heap_are_placed)
{ const VkMemoryType types[] = {{local, 0u}, {visible, 1u}, {local, 0u}, {visible, 1u}};
  CHECK_EQ(PlacedMemoryTypes::mask(made(types, 4u)), (1u << 1u) | (1u << 3u)); }

TEST(placed_memory_types, a_twin_in_another_heap_or_with_other_flags_does_not_count_and_a_pair_without_host_access_is_not_placed)
{ const VkMemoryType types[] = {{visible, 0u}, {visible, 1u}, {visible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 1u},
                                {local, 0u}, {local, 0u}};
  CHECK_EQ(PlacedMemoryTypes::mask(made(types, 5u)), 0u); }

TEST(placed_memory_types, a_count_past_the_array_is_read_no_further_than_the_array)
{ VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = 0xFFFFu;
  for (std::uint32_t index = 0u; index < VK_MAX_MEMORY_TYPES; index++) properties.memoryTypes[index] = {visible, 1u};
  CHECK_EQ(PlacedMemoryTypes::mask(properties), 0xFFFFFFFFu); }
