#include "harness/framework/test_registry.hpp"
#include "proto/client/client_blob_arena.hpp"

#include <cstdint>

using av::ClientBlobArena;
using av::ClientBlobLease;

namespace {

constexpr std::uint64_t slot = std::uint64_t{1} << 26u;

}

TEST(client_blob_arena, an_arena_with_no_segment_is_never_ready)
{ ClientBlobArena arena;
  CHECK(!arena.ready());
  CHECK(!arena.take(16u).taken());
  CHECK_EQ(arena.segments(), 0u);
  CHECK_EQ(arena.room(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_single_lent_block_is_one_segment_at_offset_zero)
{ std::uint8_t block[256];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.ready());
  CHECK_EQ(arena.segments(), 1u);
  CHECK_EQ(arena.segment_base(0u), 0u);
  CHECK_EQ(arena.segment_bytes(0u), sizeof block);
  const ClientBlobLease first = arena.take(64u);
  CHECK(first.data() == block);
  CHECK_EQ(first.offset(), 0u);
  CHECK_EQ(arena.take(32u).offset(), 64u);
  CHECK_EQ(arena.used(), 96u);
  arena.lend(nullptr, 0u);
  CHECK(!arena.ready());
  CHECK_EQ(arena.lent(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_second_segment_names_its_blobs_from_its_own_base)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.invariants_hold());
  CHECK_EQ(arena.segments(), 2u);
  CHECK(arena.take(64u).data() == first);
  const ClientBlobLease spilled = arena.take(40u);
  CHECK(spilled.taken());
  CHECK(spilled.data() == second);
  CHECK_EQ(spilled.offset(), slot);
  const ClientBlobLease next = arena.take(8u);
  CHECK_EQ(next.offset(), slot + 40u);
  CHECK_EQ(arena.used(), 112u);
  CHECK(!arena.take(32u).taken());
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_blob_is_contiguous_within_one_segment_so_two_halves_do_not_make_a_whole)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.take(40u).taken());
  CHECK(arena.take(40u).taken());
  CHECK_EQ(arena.room(), 24u);
  CHECK(!arena.take(48u).taken());
  CHECK(arena.take(24u).taken()); }

TEST(client_blob_arena, a_lease_given_back_returns_to_the_segment_that_made_it)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.take(64u).taken());
  const ClientBlobLease top = arena.take(16u);
  CHECK_EQ(top.offset(), slot);
  arena.give_back(top);
  CHECK_EQ(arena.used(), 64u);
  CHECK_EQ(arena.take(16u).offset(), slot);
  arena.give_back(ClientBlobLease());
  arena.give_back(ClientBlobLease(second, slot * 5u, 16u));
  CHECK_EQ(arena.used(), 80u);
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_mark_retires_every_segment_that_the_host_has_consumed)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.take(64u).taken());
  CHECK(arena.take(64u).taken());
  CHECK_EQ(arena.room(), 0u);
  arena.seal();
  arena.mark(1u);
  CHECK_EQ(arena.room(), 0u);
  arena.retire_through(1u);
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.room(), 64u);
  CHECK(arena.segment_idle(0u));
  CHECK(arena.segment_idle(1u));
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_segment_still_holding_data_is_not_idle_and_will_not_be_removed)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.take(64u).taken());
  CHECK(arena.take(8u).taken());
  CHECK(!arena.segment_idle(1u));
  CHECK(!arena.remove_segment(slot));
  arena.seal();
  arena.mark(1u);
  arena.retire_through(1u);
  CHECK(arena.remove_segment(slot));
  CHECK_EQ(arena.segments(), 1u);
  CHECK(!arena.remove_segment(slot));
  CHECK(!arena.remove_segment(7u * slot));
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_removed_segment_leaves_its_offsets_unnamed_and_its_slot_free_to_reuse)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  std::uint8_t third[32];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.remove_segment(slot));
  std::uint64_t offset = 0u;
  CHECK(!arena.holds(second, 8u, offset));
  CHECK(arena.add_segment(slot, third, sizeof third));
  const ClientBlobLease lease = arena.take(64u);
  CHECK(lease.data() == first);
  CHECK_EQ(arena.take(32u).offset(), slot);
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, overlapping_null_empty_and_surplus_segments_are_refused)
{ std::uint8_t first[64];
  std::uint8_t other[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(!arena.add_segment(32u, other, sizeof other));
  CHECK(!arena.add_segment(0u, other, sizeof other));
  CHECK(!arena.add_segment(slot, nullptr, sizeof other));
  CHECK(!arena.add_segment(slot, other, 0u));
  CHECK(!arena.add_segment(~std::uint64_t{0}, other, sizeof other));
  CHECK_EQ(arena.segments(), 1u);
  for (std::uint32_t index = 1u; index < ClientBlobArena::max_segments; index++)
      CHECK(arena.add_segment(slot * index, other, sizeof other));
  CHECK_EQ(arena.segments(), ClientBlobArena::max_segments);
  CHECK(!arena.add_segment(slot * ClientBlobArena::max_segments, other, sizeof other));
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, lending_again_drops_every_segment_and_region)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  std::uint8_t region[32];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.add_region(slot * 2u, region, sizeof region));
  CHECK(arena.take(8u).taken());
  arena.lend(second, sizeof second);
  CHECK_EQ(arena.segments(), 1u);
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.set_aside_bytes(), 0u);
  CHECK_EQ(arena.peak_ring(), 0u);
  CHECK(arena.take(8u).data() == second);
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, fits_places_each_blob_in_a_segment_of_its_own_by_size)
{ std::uint8_t first[64];
  std::uint8_t second[32];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  const std::uint64_t one[] = {64u};
  const std::uint64_t two[] = {40u, 32u};
  const std::uint64_t three[] = {40u, 40u};
  const std::uint64_t none[] = {0u, 0u};
  const std::uint64_t huge[] = {~std::uint64_t{0}};
  CHECK(arena.fits(one, 1u));
  CHECK(arena.fits(two, 2u));
  CHECK(!arena.fits(three, 2u));
  CHECK(arena.fits(none, 2u));
  CHECK(!arena.fits(huge, 1u));
  CHECK(arena.fits(nullptr, 0u)); }

TEST(client_blob_arena, fits_counts_what_earlier_blobs_of_the_same_command_will_take)
{ std::uint8_t first[64];
  ClientBlobArena arena(first, sizeof first);
  const std::uint64_t each[] = {32u, 32u};
  const std::uint64_t over[] = {32u, 32u, 1u};
  CHECK(arena.fits(each, 2u));
  CHECK(!arena.fits(over, 3u));
  CHECK(arena.take(1u).taken());
  CHECK(!arena.fits(each, 2u)); }

TEST(client_blob_arena, fits_places_from_the_segment_the_next_take_starts_at)
{ std::uint8_t first[10];
  std::uint8_t second[11];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.take(11u).taken());
  arena.seal();
  arena.mark(1u);
  arena.retire_through(1u);
  CHECK(arena.take(5u).offset() == slot);
  const std::uint64_t wanted[] = {5u, 5u, 6u};
  CHECK(!arena.fits(wanted, 3u));
  CHECK(arena.take(5u).taken());
  CHECK(arena.take(5u).taken());
  CHECK(!arena.take(6u).taken());
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_pointer_is_held_in_any_segment_or_region_and_named_by_its_arena_offset)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  std::uint8_t region[128];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.add_region(slot * 3u, region, sizeof region));
  std::uint64_t offset = 999u;
  CHECK(arena.holds(first + 10, 4u, offset));
  CHECK_EQ(offset, 10u);
  CHECK(arena.holds(second + 60, 4u, offset));
  CHECK_EQ(offset, slot + 60u);
  CHECK(arena.holds(region + 100, 28u, offset));
  CHECK_EQ(offset, slot * 3u + 100u);
  offset = 999u;
  CHECK(!arena.holds(region + 100, 29u, offset));
  CHECK(!arena.holds(region, 0u, offset));
  CHECK(!arena.holds(second + 64, 1u, offset));
  CHECK_EQ(offset, 999u); }

TEST(client_blob_arena, regions_are_counted_as_set_aside_and_are_never_leased_from)
{ std::uint8_t first[64];
  std::uint8_t region[128];
  std::uint8_t more[32];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_region(slot, region, sizeof region));
  CHECK(arena.add_region(slot * 2u, more, sizeof more));
  CHECK_EQ(arena.set_aside_bytes(), 160u);
  CHECK_EQ(arena.lent(), 64u + 160u);
  CHECK_EQ(arena.ring_bytes(), 64u);
  CHECK(!arena.take(65u).taken());
  CHECK(!arena.remove_region(slot * 5u));
  CHECK(arena.remove_region(slot));
  CHECK_EQ(arena.set_aside_bytes(), 32u);
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_region_may_not_overlap_a_segment_a_region_or_wrap)
{ std::uint8_t first[64];
  std::uint8_t region[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(!arena.add_region(16u, region, sizeof region));
  CHECK(arena.add_region(slot, region, sizeof region));
  CHECK(!arena.add_region(slot + 32u, region, sizeof region));
  CHECK(!arena.add_region(slot, region, sizeof region));
  CHECK(!arena.add_region(slot * 2u, nullptr, 8u));
  CHECK(!arena.add_region(slot * 2u, region, 0u));
  CHECK(!arena.add_region(~std::uint64_t{0}, region, sizeof region));
  CHECK(!arena.add_segment(slot, first, sizeof first));
  for (std::uint32_t index = 1u; index < ClientBlobArena::max_regions; index++)
      CHECK(arena.add_region(slot * (index + 1u), region, sizeof region));
  CHECK(!arena.add_region(slot * (ClientBlobArena::max_regions + 1u), region, sizeof region));
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, the_peak_is_the_most_leased_across_all_segments_at_once)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.take(48u).taken());
  CHECK(arena.take(48u).taken());
  CHECK_EQ(arena.peak_ring(), 96u);
  arena.seal();
  arena.mark(1u);
  arena.retire_through(1u);
  CHECK(arena.take(8u).taken());
  CHECK_EQ(arena.peak_ring(), 96u); }

TEST(client_blob_arena, idle_at_names_a_segment_by_its_base_and_is_true_only_while_nothing_is_leased)
{ std::uint8_t first[64];
  std::uint8_t second[64];
  ClientBlobArena arena(first, sizeof first);
  CHECK(arena.add_segment(slot, second, sizeof second));
  CHECK(arena.idle_at(0u));
  CHECK(arena.idle_at(slot));
  CHECK(!arena.idle_at(slot * 2u));
  CHECK(!arena.idle_at(8u));
  CHECK(arena.take(64u).taken());
  CHECK(arena.take(8u).taken());
  CHECK(!arena.idle_at(0u));
  CHECK(!arena.idle_at(slot));
  arena.seal();
  arena.mark(1u);
  arena.retire_through(1u);
  CHECK(arena.idle_at(0u));
  CHECK(arena.idle_at(slot));
  CHECK(arena.remove_segment(slot));
  CHECK(!arena.idle_at(slot)); }
