/* The memory types a Vulkan guest sees on one host GPU, and the host type
 * behind each (docs/host-memory.md). The layouts are the GPUs this project
 * runs on, as their drivers report them: RADV's discrete four, NVIDIA's five
 * with a BAR the source cannot map, and ANV's Meteor Lake five - one heap,
 * every usable type DEVICE_LOCAL|HOST_VISIBLE, a protected one among them.
 * What a caller acts on: the types and heaps the guest is answered, which
 * host type a guest type is allocated as and whether the source allocates
 * it, and the memoryTypeBits a host requirement becomes. */
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

/* `count` types on `heaps` heaps, the first device local. */
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

/* The guest's types as (flags, heap, host type, source type), in order. */
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

/* RADV on the RX 9070 XT: VRAM, GTT, the BAR and cached GTT. */
const Type radv[] = {{local, 0u}, {visible | coherent, 1u}, {local | visible | coherent, 0u},
                     {visible | coherent | cached, 1u}};
/* ANV on Meteor Lake: one heap; 3 and 4 serve buffers only. */
const Type anv[] = {{local | visible | coherent, 0u}, {local | visible | cached, 0u}, {local | protected_memory, 0u},
                    {local | visible | coherent, 0u}, {local | visible | cached, 0u}};

}  // namespace

TEST(guest_memory_types, shows_nothing_before_it_learns)
{ GuestMemoryTypes types;
  CHECK(types.invariants_hold());
  const VkPhysicalDeviceMemoryProperties guest = shown(types);
  CHECK_EQ(guest.memoryTypeCount, 0u);
  CHECK_EQ(guest.memoryHeapCount, 0u);
  CHECK_EQ(types.host_type(0u), none);
  CHECK_EQ(types.source_type(0u), none);
  CHECK_EQ(types.guest_bits(0xFFFFFFFFu), 0u); }

/* A GPU with a device-local type of its own and every host-visible type
 * mapped is the host's layout unchanged: same types, same order, same heaps. */
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

/* NVIDIA's BAR exports but its dma-buf will not map: it loses HOST_VISIBLE,
 * and with it HOST_COHERENT, and is a second device-local type; the
 * heap already had one, so nothing is added, and nothing moves. */
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

/* ANV's every usable type is host visible: the heap gains a type that is
 * DEVICE_LOCAL alone, before the type it was derived from, allocated as that
 * host type and never by the source. The protected type is not one the heap
 * could use instead, and keeps its place. */
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

/* A host bit with a twin carries the twin with it: an optimal image ANV
 * places in 0 and 1 may be placed in the guest's 0, 1 and 2, and one
 * offered the protected type is offered it at its guest index. */
TEST(guest_memory_types, host_bits_are_the_guest_types_behind_them_twins_included)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  CHECK_EQ(types.guest_bits(0x3u), 0x7u);
  CHECK_EQ(types.guest_bits(0x1Bu), 0x37u);
  CHECK_EQ(types.guest_bits(0x4u), 0x8u);
  CHECK_EQ(types.guest_bits(0x0u), 0x0u);
  /* Host types the GPU does not have are no guest's. */
  CHECK_EQ(types.guest_bits(0xFFFFFFE0u), 0x0u); }

/* Memory the guest never maps is what an image not made for the source is
 * bound to: the types the source backs go, unless nothing else is left -
 * memory a requirement only the source's types answer is still memory. */
TEST(guest_memory_types, unmapped_keeps_only_native_types_unless_none_is_left)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  CHECK_EQ(types.unmapped(0x7u), 0x1u);
  CHECK_EQ(types.unmapped(0x3Fu), 0x9u);
  CHECK_EQ(types.unmapped(0x6u), 0x6u);
  CHECK_EQ(types.unmapped(0x0u), 0x0u);
  CHECK_EQ(types.unmapped(0x8u), 0x8u); }

/* With no source, nothing is host visible and every device-local type of
 * ANV's is plain device local: the heap is served, no type is added, and the
 * protected type - a strict superset of the plain ones - goes after them. */
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

/* A type left with no flags at all precedes every type with any, and the
 * others keep the host's order among themselves. */
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

/* Each heap is judged on its own: one with a device-local type of its own
 * gains nothing, each without one gains its own, derived from its first
 * type the source backs. */
TEST(guest_memory_types, every_heap_without_device_local_memory_gains_its_own)
{ const Type gpu[] = {{local, 0u}, {local | visible | coherent, 0u}, {local | visible | coherent, 1u},
                      {visible | coherent, 2u}, {local | visible | coherent | cached, 3u}};
  GuestMemoryTypes types;
  types.learn(host(gpu, 4u), 0x1Eu);
  /* Each step takes the first type nothing left must precede: both twins
   * come before every device-local host-visible type, and the system heap's
   * type, which no twin precedes, before the second twin. */
  const Expected expected[] = {{local, 0u, 0u, none},
                               {local, 1u, 2u, none},
                               {visible | coherent, 2u, 3u, 3u},
                               {local, 3u, 4u, none},
                               {local | visible | coherent, 0u, 1u, 1u},
                               {local | visible | coherent, 1u, 2u, 2u},
                               {local | visible | coherent | cached, 3u, 4u, 4u}};
  check_types(types, expected); }

/* A twin is plain device-local memory: one derived from a type with more
 * than host access - AMD's device-coherent memory - would not be. A type the
 * source does not back is no origin either, so the first backed type that
 * qualifies is. */
TEST(guest_memory_types, a_twin_is_derived_only_from_a_backed_type_with_nothing_but_host_access)
{ const Type gpu[] = {{local | visible | coherent | amd_coherent, 0u}, {local | visible | cached, 0u},
                      {local | visible | coherent, 0u}};
  GuestMemoryTypes types;
  types.learn(host(gpu, 1u), 0x5u);
  /* 1 is not backed, so the guest sees it as device-local memory alone,
   * which serves the heap: nothing is added. */
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

/* The protected type neither serves a heap nor is a twin's origin. */
TEST(guest_memory_types, a_protected_type_neither_serves_a_heap_nor_derives_a_twin)
{ const Type gpu[] = {{local | protected_memory, 0u}, {local | visible | coherent, 0u}};
  GuestMemoryTypes types;
  types.learn(host(gpu, 1u), 0x2u);
  const Expected expected[] = {{local, 0u, 1u, none},
                               {local | protected_memory, 0u, 0u, none},
                               {local | visible | coherent, 0u, 1u, 1u}};
  check_types(types, expected); }

/* Vulkan has room for thirty-two types: a host with all of them gains none,
 * and one with thirty-one gains one, for the first heap that needs it. */
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

/* The source maps only host-visible memory: a mask naming any other type
 * backs nothing there, and nothing is derived from it. */
TEST(guest_memory_types, a_mask_naming_a_type_that_is_not_host_visible_backs_nothing_there)
{ const Type gpu[] = {{local, 0u}, {visible | coherent, 1u}};
  GuestMemoryTypes types;
  types.learn(host(gpu), 0xFFFFFFFFu);
  const Expected expected[] = {{local, 0u, 0u, none}, {visible | coherent, 1u, 1u, 1u}};
  check_types(types, expected); }

/* The counts are the driver's and never trusted past Vulkan's arrays. */
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

/* A guest index the view does not have names no host type, whatever its
 * value: the host's driver checks no index, so this is the only check. */
TEST(guest_memory_types, an_index_past_the_guest_s_types_names_none)
{ GuestMemoryTypes types;
  types.learn(host(anv, 1u), 0x1Bu);
  constexpr std::uint32_t outside[] = {6u, 31u, 32u, 0xFFFFFFFFu};
  for (const std::uint32_t type : outside) {
      CHECK_EQ(types.host_type(type), none);
      CHECK_EQ(types.source_type(type), none);
  }
  CHECK_EQ(types.host_type(5u), 4u); }

/* Learning again is a new view: nothing of the old one stays behind. */
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

}  // namespace

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
