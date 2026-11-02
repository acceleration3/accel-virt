#include "executor/vulkan/guest_memory_types.hpp"
#include "harness/executor/guest_memory_types_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GuestMemoryTypes;
using av::GuestMemoryTypesProbe;

namespace {

constexpr VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
constexpr VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
constexpr VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
constexpr VkMemoryPropertyFlags cached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
constexpr VkMemoryPropertyFlags protected_memory = VK_MEMORY_PROPERTY_PROTECTED_BIT;
constexpr VkMemoryPropertyFlags amd_coherent =
    VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;
constexpr std::uint32_t none = GuestMemoryTypes::no_type;

struct Type {
    VkMemoryPropertyFlags flags;
    std::uint32_t         heap;
};

template <std::uint32_t count>
VkPhysicalDeviceMemoryProperties host(const Type (&types)[count], std::uint32_t heaps = 2u)
{ VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = count;
  for (std::uint32_t type = 0u; type < count; type++)
      properties.memoryTypes[type] = VkMemoryType{types[type].flags, types[type].heap};
  properties.memoryHeapCount = heaps;
  for (std::uint32_t heap = 0u; heap < heaps; heap++)
      properties.memoryHeaps[heap] = VkMemoryHeap{(heap + 1u) * 0x10000000ull,
                                                  heap == 0u ? VkMemoryHeapFlags{VK_MEMORY_HEAP_DEVICE_LOCAL_BIT} : 0u};
  return properties; }

VkPhysicalDeviceMemoryProperties shown(const GuestMemoryTypes &types)
{ VkPhysicalDeviceMemoryProperties properties = {};
  types.properties(properties);
  return properties; }

struct Expected {
    VkMemoryPropertyFlags flags;
    std::uint32_t         heap;
    std::uint32_t         host;
    std::uint32_t         source;
};

template <std::uint32_t count>
void check_types(const GuestMemoryTypes &types, const Expected (&expected)[count])
{ const VkPhysicalDeviceMemoryProperties guest = shown(types);
  CHECK_EQ(guest.memoryTypeCount, count);
  for (std::uint32_t type = 0u; type < count; type++) {
      CHECK_EQ(guest.memoryTypes[type].propertyFlags, expected[type].flags);
      CHECK_EQ(guest.memoryTypes[type].heapIndex, expected[type].heap);
      CHECK_EQ(types.host_type(type), expected[type].host);
      CHECK_EQ(types.source_type(type), expected[type].source);
  }
  CHECK(types.invariants_hold()); }

const Type radv[] = {{local, 0u}, {visible | coherent, 1u}, {local | visible | coherent, 0u},
                     {visible | coherent | cached, 1u}};
const Type anv[] = {{local | visible | coherent, 0u}, {local | visible | cached, 0u}, {local | protected_memory, 0u},
                    {local | visible | coherent, 0u}, {local | visible | cached, 0u}};

}

TEST(guest_memory_types, shows_nothing_before_it_learns)
{ GuestMemoryTypes types;
  CHECK(types.invariants_hold());
  const VkPhysicalDeviceMemoryProperties guest = shown(types);
  CHECK_EQ(guest.memoryTypeCount, 0u);
  CHECK_EQ(guest.memoryHeapCount, 0u);
  CHECK_EQ(types.host_type(0u), none);
  CHECK_EQ(types.source_type(0u), none);
  CHECK_EQ(types.guest_bits(0xFFFFFFFFu), 0u); }

TEST(guest_memory_types, a_discrete_gpu_whose_types_the_source_maps_is_shown_as_it_is)
{ const VkPhysicalDeviceMemoryProperties gpu = host(radv);
  GuestMemoryTypes types;
  types.learn(gpu, 0xEu);
  const Expected expected[] = {{local, 0u, 0u, none},
                               {visible | coherent, 1u, 1u, 1u},
                               {local | visible | coherent, 0u, 2u, 2u},
                               {visible | coherent | cached, 1u, 3u, 3u}};
  check_types(types, expected);
  const VkPhysicalDeviceMemoryProperties guest = shown(types);
  CHECK_EQ(guest.memoryHeapCount, 2u);
  CHECK_EQ(guest.memoryHeaps[1].size, gpu.memoryHeaps[1].size);
  CHECK_EQ(guest.memoryHeaps[0].flags, VkMemoryHeapFlags{VK_MEMORY_HEAP_DEVICE_LOCAL_BIT});
  CHECK_EQ(types.guest_bits(0x5u), 0x5u);
  CHECK_EQ(types.guest_bits(0xFu), 0xFu); }

TEST(guest_memory_types, a_type_the_source_cannot_map_loses_host_access_and_gains_nothing)
{ const Type nvidia[] = {{0u, 1u}, {local, 0u}, {visible | coherent, 1u}, {visible | coherent | cached, 1u},
                         {local | visible | coherent, 0u}};
  GuestMemoryTypes types;
  types.learn(host(nvidia), 0xCu);
  const Expected expected[] = {{0u, 1u, 0u, none},
                               {local, 0u, 1u, none},
                               {visible | coherent, 1u, 2u, 2u},
                               {visible | coherent | cached, 1u, 3u, 3u},
                               {local, 0u, 4u, none}};
  check_types(types, expected); }

TEST(guest_memory_types, an_integrated_gpu_gains_a_device_local_type_before_its_host_visible_ones)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  const Expected expected[] = {{local, 0u, 0u, none},
                               {local | visible | coherent, 0u, 0u, 0u},
                               {local | visible | cached, 0u, 1u, 1u},
                               {local | protected_memory, 0u, 2u, none},
                               {local | visible | coherent, 0u, 3u, 3u},
                               {local | visible | cached, 0u, 4u, 4u}};
  check_types(types, expected);
  CHECK_EQ(shown(types).memoryHeapCount, 1u); }

TEST(guest_memory_types, host_bits_are_the_guest_types_behind_them_twins_included)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  CHECK_EQ(types.guest_bits(0x3u), 0x7u);
  CHECK_EQ(types.guest_bits(0x1Bu), 0x37u);
  CHECK_EQ(types.guest_bits(0x4u), 0x8u);
  CHECK_EQ(types.guest_bits(0x0u), 0x0u);
  CHECK_EQ(types.guest_bits(0xFFFFFFE0u), 0x0u); }

TEST(guest_memory_types, unmapped_keeps_only_native_types_unless_none_is_left)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  CHECK_EQ(types.unmapped(0x7u), 0x1u);
  CHECK_EQ(types.unmapped(0x3Fu), 0x9u);
  CHECK_EQ(types.unmapped(0x6u), 0x6u);
  CHECK_EQ(types.unmapped(0x0u), 0x0u);
  CHECK_EQ(types.unmapped(0x8u), 0x8u); }

TEST(guest_memory_types, with_nothing_backed_no_type_is_added_and_the_order_still_holds)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0u);
  const Expected expected[] = {{local, 0u, 0u, none},
                               {local, 0u, 1u, none},
                               {local, 0u, 3u, none},
                               {local, 0u, 4u, none},
                               {local | protected_memory, 0u, 2u, none}};
  check_types(types, expected);
  CHECK_EQ(types.guest_bits(0x4u), 0x10u);
  CHECK_EQ(types.guest_bits(0x8u), 0x4u); }

TEST(guest_memory_types, a_type_left_with_no_flags_comes_before_every_other)
{ const Type gpu[] = {{local, 0u}, {visible | coherent, 1u}, {visible | coherent | cached, 1u}};
  GuestMemoryTypes types;
  types.learn(host(gpu), 0x4u);
  const Expected expected[] = {{0u, 1u, 1u, none},
                               {local, 0u, 0u, none},
                               {visible | coherent | cached, 1u, 2u, 2u}};
  check_types(types, expected);
  CHECK_EQ(types.guest_bits(0x1u), 0x2u);
  CHECK_EQ(types.guest_bits(0x2u), 0x1u); }

TEST(guest_memory_types, every_heap_without_device_local_memory_gains_its_own)
{ const Type gpu[] = {{local, 0u}, {local | visible | coherent, 0u}, {local | visible | coherent, 1u},
                      {visible | coherent, 2u}, {local | visible | coherent | cached, 3u}};
  GuestMemoryTypes types;
  types.learn(host(gpu, 4u), 0x1Eu);
  const Expected expected[] = {{local, 0u, 0u, none},
                               {local, 1u, 2u, none},
                               {visible | coherent, 2u, 3u, 3u},
                               {local, 3u, 4u, none},
                               {local | visible | coherent, 0u, 1u, 1u},
                               {local | visible | coherent, 1u, 2u, 2u},
                               {local | visible | coherent | cached, 3u, 4u, 4u}};
  check_types(types, expected); }

TEST(guest_memory_types, a_twin_is_derived_only_from_a_backed_type_with_nothing_but_host_access)
{ const Type gpu[] = {{local | visible | coherent | amd_coherent, 0u}, {local | visible | cached, 0u},
                      {local | visible | coherent, 0u}};
  GuestMemoryTypes types;
  types.learn(host(gpu, 1u), 0x5u);
  const Expected served[] = {{local, 0u, 1u, none},
                             {local | visible | coherent, 0u, 2u, 2u},
                             {local | visible | coherent | amd_coherent, 0u, 0u, 0u}};
  check_types(types, served);

  GuestMemoryTypes every;
  every.learn(host(gpu, 1u), 0x7u);
  const Expected derived[] = {{local, 0u, 1u, none},
                              {local | visible | cached, 0u, 1u, 1u},
                              {local | visible | coherent, 0u, 2u, 2u},
                              {local | visible | coherent | amd_coherent, 0u, 0u, 0u}};
  check_types(every, derived);

  const Type amd_only[] = {{local | visible | coherent | amd_coherent, 0u}};
  GuestMemoryTypes none_derived;
  none_derived.learn(host(amd_only, 1u), 0x1u);
  const Expected alone[] = {{local | visible | coherent | amd_coherent, 0u, 0u, 0u}};
  check_types(none_derived, alone); }

TEST(guest_memory_types, a_protected_type_neither_serves_a_heap_nor_derives_a_twin)
{ const Type gpu[] = {{local | protected_memory, 0u}, {local | visible | coherent, 0u}};
  GuestMemoryTypes types;
  types.learn(host(gpu, 1u), 0x2u);
  const Expected expected[] = {{local, 0u, 1u, none},
                               {local | protected_memory, 0u, 0u, none},
                               {local | visible | coherent, 0u, 1u, 1u}};
  check_types(types, expected); }

TEST(guest_memory_types, twins_are_added_only_while_vulkan_has_room_for_them)
{ VkPhysicalDeviceMemoryProperties full = {};
  full.memoryTypeCount = VK_MAX_MEMORY_TYPES;
  full.memoryHeapCount = 2u;
  for (std::uint32_t type = 0u; type < VK_MAX_MEMORY_TYPES; type++)
      full.memoryTypes[type] = VkMemoryType{local | visible | coherent, type % 2u};
  GuestMemoryTypes types;
  types.learn(full, 0xFFFFFFFFu);
  CHECK_EQ(shown(types).memoryTypeCount, VK_MAX_MEMORY_TYPES);
  CHECK_EQ(types.source_type(0u), 0u);
  CHECK_EQ(types.source_type(31u), 31u);
  CHECK(types.invariants_hold());

  full.memoryTypeCount = VK_MAX_MEMORY_TYPES - 1u;
  types.learn(full, 0xFFFFFFFFu);
  const VkPhysicalDeviceMemoryProperties guest = shown(types);
  CHECK_EQ(guest.memoryTypeCount, VK_MAX_MEMORY_TYPES);
  CHECK_EQ(guest.memoryTypes[0].propertyFlags, local);
  CHECK_EQ(guest.memoryTypes[0].heapIndex, 0u);
  CHECK_EQ(types.host_type(0u), 0u);
  for (std::uint32_t type = 1u; type < VK_MAX_MEMORY_TYPES; type++) {
      CHECK_EQ(guest.memoryTypes[type].propertyFlags, local | visible | coherent);
      CHECK_EQ(types.source_type(type), type - 1u);
  }
  CHECK(types.invariants_hold()); }

TEST(guest_memory_types, a_mask_naming_a_type_that_is_not_host_visible_backs_nothing_there)
{ const Type gpu[] = {{local, 0u}, {visible | coherent, 1u}};
  GuestMemoryTypes types;
  types.learn(host(gpu), 0xFFFFFFFFu);
  const Expected expected[] = {{local, 0u, 0u, none}, {visible | coherent, 1u, 1u, 1u}};
  check_types(types, expected); }

TEST(guest_memory_types, counts_past_the_arrays_are_cut_to_them)
{ VkPhysicalDeviceMemoryProperties gpu = {};
  gpu.memoryTypeCount = 0xFFFFFFFFu;
  gpu.memoryHeapCount = 0xFFFFFFFFu;
  for (auto &type : gpu.memoryTypes) type = VkMemoryType{local, 0u};
  GuestMemoryTypes types;
  types.learn(gpu, 0u);
  const VkPhysicalDeviceMemoryProperties guest = shown(types);
  CHECK_EQ(guest.memoryTypeCount, VK_MAX_MEMORY_TYPES);
  CHECK_EQ(guest.memoryHeapCount, VK_MAX_MEMORY_HEAPS);
  CHECK_EQ(types.host_type(31u), 31u);
  CHECK_EQ(types.guest_bits(0x80000000u), 0x80000000u);
  CHECK(types.invariants_hold()); }

TEST(guest_memory_types, an_index_past_the_guest_s_types_names_none)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  constexpr std::uint32_t outside[] = {6u, 31u, 32u, 0xFFFFFFFFu};
  for (const std::uint32_t type : outside) {
      CHECK_EQ(types.host_type(type), none);
      CHECK_EQ(types.source_type(type), none);
  }
  CHECK_EQ(types.host_type(5u), 4u); }

TEST(guest_memory_types, learning_again_replaces_the_view)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  types.learn(host(radv), 0xEu);
  const Expected expected[] = {{local, 0u, 0u, none},
                               {visible | coherent, 1u, 1u, 1u},
                               {local | visible | coherent, 0u, 2u, 2u},
                               {visible | coherent | cached, 1u, 3u, 3u}};
  check_types(types, expected);
  CHECK_EQ(types.host_type(4u), none);
  CHECK_EQ(types.unmapped(0xFu), 0x1u); }

namespace {

GuestMemoryTypes &learnt_anv(GuestMemoryTypes &types)
{ types.learn(host(anv, 1u), 0x1Bu);
  return types; }

}

TEST(guest_memory_types_invariants, a_count_past_the_arrays_breaks_them)
{ GuestMemoryTypes types;
  GuestMemoryTypesProbe(learnt_anv(types)).set_type_count(VK_MAX_MEMORY_TYPES + 1u);
  CHECK(!types.invariants_hold());
  GuestMemoryTypes heaps;
  GuestMemoryTypesProbe(learnt_anv(heaps)).set_heap_count(VK_MAX_MEMORY_HEAPS + 1u);
  CHECK(!heaps.invariants_hold()); }

TEST(guest_memory_types_invariants, more_host_types_than_guest_types_break_them)
{ GuestMemoryTypes types;
  GuestMemoryTypesProbe(learnt_anv(types)).set_host_types(7u);
  CHECK(!types.invariants_hold()); }

TEST(guest_memory_types_invariants, a_backed_type_past_the_count_breaks_them)
{ GuestMemoryTypes types;
  GuestMemoryTypesProbe(learnt_anv(types)).set_backed(0x36u | 0x40u);
  CHECK(!types.invariants_hold()); }

TEST(guest_memory_types_invariants, a_host_type_the_gpu_does_not_have_breaks_them)
{ GuestMemoryTypes types;
  GuestMemoryTypesProbe(learnt_anv(types)).set_host(2u, 5u);
  CHECK(!types.invariants_hold()); }

TEST(guest_memory_types_invariants, host_visibility_without_the_source_or_the_source_without_it_breaks_them)
{ GuestMemoryTypes hidden;
  GuestMemoryTypesProbe(learnt_anv(hidden)).set_backed(0x36u | 0x1u);
  CHECK(!hidden.invariants_hold());
  GuestMemoryTypes unbacked;
  GuestMemoryTypesProbe(learnt_anv(unbacked)).set_backed(0x36u & ~0x2u);
  CHECK(!unbacked.invariants_hold()); }

TEST(guest_memory_types_invariants, a_type_after_one_it_should_precede_breaks_them)
{ GuestMemoryTypes types;
  GuestMemoryTypesProbe(learnt_anv(types)).set_flags(3u, 0u);
  CHECK(!types.invariants_hold()); }

TEST(guest_memory_types_invariants, a_host_type_shown_by_no_guest_type_breaks_them)
{ GuestMemoryTypes types;
  GuestMemoryTypesProbe(learnt_anv(types)).set_host(3u, 0u);
  CHECK(!types.invariants_hold()); }
