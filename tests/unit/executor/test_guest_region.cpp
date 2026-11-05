#include "executor/exported_memory.hpp"
#include "executor/guest_region.hpp"
#include "executor/source_memory.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/blob_role.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>

using av::BlobRole;
using av::ExportedMemory;
using av::GuestBytes;
using av::GuestRegion;
using av::SourceMemory;

namespace {

alignas(4096) std::uint8_t mapping[16384];

class Memory {
public:
    Memory(std::uint8_t *pointer, std::uint64_t size) : memory(1u, 0x40u, 0u, 1u, ExportedMemory(0x99u, -1, pointer, size)) {}

    SourceMemory memory;
};

BlobRole arena_at(std::uint32_t slot)
{
    return BlobRole(BlobRole::Kind::arena_segment, std::uint64_t{slot} << GuestBytes::slot_shift);
}

std::uint64_t address_of(const void *pointer)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

}

TEST(guest_region_adopt, an_arena_import_becomes_a_segment_named_by_its_address)
{ GuestBytes bytes;
  GuestRegion region(bytes);
  Memory      import(mapping, sizeof mapping);
  std::uint64_t handle = 1u;
  CHECK(!region.held());
  CHECK(region.adopt(import.memory, arena_at(0u), handle));
  CHECK_EQ(handle, address_of(mapping));
  CHECK(region.held());
  CHECK(region.is(import.memory));
  CHECK_EQ(region.base_of(import.memory), 0u);
  CHECK(bytes.at(0u, sizeof mapping) == mapping);
  CHECK(region.memory_at(100u) == &import.memory);
  CHECK(region.invariants_hold()); }

TEST(guest_region_adopt, a_plain_role_is_not_a_segment)
{ GuestBytes bytes;
  GuestRegion region(bytes);
  Memory      import(mapping, sizeof mapping);
  std::uint64_t handle = 1u;
  CHECK(!region.adopt(import.memory, BlobRole(), handle));
  CHECK_EQ(handle, 0u);
  CHECK(!region.held());
  CHECK(region.invariants_hold()); }

TEST(guest_region_adopt, segments_adopted_in_any_order_land_at_their_roles_bases)
{ GuestBytes bytes;
  GuestRegion region(bytes);
  Memory      later(mapping + 8192, 4096u);
  Memory      earlier(mapping, 4096u);
  std::uint64_t handle = 0u;
  CHECK(region.adopt(later.memory, arena_at(2u), handle));
  CHECK(region.adopt(earlier.memory, arena_at(0u), handle));
  CHECK(region.memory_at(GuestBytes::slot_bytes * 2u) == &later.memory);
  CHECK(region.memory_at(0u) == &earlier.memory);
  CHECK(region.memory_at(GuestBytes::slot_bytes) == nullptr);
  CHECK(bytes.at(GuestBytes::slot_bytes * 2u, 4096u) == mapping + 8192);
  CHECK(region.invariants_hold()); }

TEST(guest_region_adopt, a_taken_slot_a_repeated_memory_or_a_bad_base_is_refused_untouched)
{ GuestBytes bytes;
  GuestRegion region(bytes);
  Memory      first(mapping, 4096u);
  Memory      second(mapping + 4096, 4096u);
  std::uint64_t handle = 0u;
  CHECK(region.adopt(first.memory, arena_at(1u), handle));
  CHECK(!region.adopt(second.memory, arena_at(1u), handle));
  CHECK_EQ(handle, 0u);
  CHECK(!region.adopt(first.memory, arena_at(2u), handle));
  CHECK(!region.adopt(second.memory, BlobRole(BlobRole::Kind::arena_segment, 4096u), handle));
  CHECK(!region.is(second.memory));
  CHECK_EQ(bytes.segments(), 1u);
  CHECK(region.invariants_hold()); }

TEST(guest_region_adopt, memory_with_no_mapping_or_no_bytes_is_refused)
{ GuestBytes bytes;
  GuestRegion region(bytes);
  Memory      unmapped(nullptr, 4096u);
  Memory      empty(mapping, 0u);
  std::uint64_t handle = 0u;
  CHECK(!region.adopt(unmapped.memory, arena_at(0u), handle));
  CHECK(!region.adopt(empty.memory, arena_at(0u), handle));
  CHECK(!region.held());
  CHECK(region.invariants_hold()); }

TEST(guest_region_abandon, only_the_adopted_memory_gives_its_segment_back)
{ GuestBytes bytes;
  GuestRegion region(bytes);
  Memory      held(mapping, 4096u);
  Memory      stranger(mapping + 4096, 4096u);
  std::uint64_t handle = 0u;
  CHECK(region.adopt(held.memory, arena_at(0u), handle));
  region.abandon(stranger.memory);
  CHECK(region.held());
  CHECK(bytes.at(0u, 4096u) == mapping);
  region.abandon(held.memory);
  CHECK(!region.held());
  CHECK(bytes.at(0u, 1u) == nullptr);
  CHECK(region.memory_at(0u) == nullptr);
  CHECK(!region.is(held.memory));
  CHECK(region.invariants_hold());
  CHECK(region.adopt(stranger.memory, arena_at(0u), handle));
  CHECK(bytes.at(0u, 4096u) == mapping + 4096);
  CHECK(region.invariants_hold()); }

TEST(guest_region_memory_at, a_long_segment_answers_with_its_memory_in_every_slot_it_covers)
{ GuestBytes bytes;
  GuestRegion region(bytes);
  Memory      long_memory(mapping, GuestBytes::slot_bytes + 4096u);
  std::uint64_t handle = 0u;
  CHECK(region.adopt(long_memory.memory, arena_at(1u), handle));
  CHECK(region.memory_at(GuestBytes::slot_bytes) == &long_memory.memory);
  CHECK(region.memory_at(GuestBytes::slot_bytes * 2u + 10u) == &long_memory.memory);
  CHECK(region.memory_at(GuestBytes::slot_bytes * 3u) == nullptr);
  CHECK(region.memory_at(~std::uint64_t{0}) == nullptr);
  CHECK(region.invariants_hold()); }
