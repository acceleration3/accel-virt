#include "guest/transport/arena_slots.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>

using av::ArenaSlots;
using av::GuestBytes;

namespace {

constexpr std::uint64_t slot = GuestBytes::slot_bytes;

}

TEST(arena_slots, nothing_is_taken_to_begin_with)
{ ArenaSlots slots;
  CHECK_EQ(slots.taken(), 0u); }

TEST(arena_slots_take, the_first_free_slot_after_the_control_slot_is_handed_out_first)
{ ArenaSlots slots;
  std::uint64_t base = 99u;
  CHECK(slots.take(4096u, base));
  CHECK_EQ(base, slot);
  CHECK(slots.take(slot, base));
  CHECK_EQ(base, 2u * slot);
  CHECK_EQ(slots.taken(), 2u); }

TEST(arena_slots_take, a_long_segment_takes_the_slots_it_covers_and_they_come_back_together)
{ ArenaSlots slots;
  std::uint64_t base = 0u;
  CHECK(slots.take(slot + 1u, base));
  CHECK_EQ(base, slot);
  CHECK_EQ(slots.taken(), 2u);
  std::uint64_t next = 0u;
  CHECK(slots.take(1u, next));
  CHECK_EQ(next, 3u * slot);
  slots.give(base, slot + 1u);
  CHECK_EQ(slots.taken(), 1u);
  CHECK(slots.take(2u * slot, next));
  CHECK_EQ(next, slot);
  CHECK_EQ(slots.taken(), 3u); }

TEST(arena_slots_take, a_run_goes_past_a_hole_that_is_too_small_for_it)
{ ArenaSlots slots;
  std::uint64_t a = 0u;
  std::uint64_t b = 0u;
  std::uint64_t c = 0u;
  CHECK(slots.take(1u, a));
  CHECK(slots.take(1u, b));
  CHECK(slots.take(1u, c));
  slots.give(b, 1u);
  std::uint64_t run = 0u;
  CHECK(slots.take(2u * slot, run));
  CHECK_EQ(run, c + slot); }

TEST(arena_slots_take, the_table_runs_out_at_sixty_four_slots_and_slot_zero_stays_for_the_control)
{ ArenaSlots slots;
  std::uint64_t base = 0u;
  for (std::uint32_t at = 1u; at < GuestBytes::slot_count; at++) {
      CHECK(slots.take(1u, base));
      CHECK_EQ(base, std::uint64_t{at} * slot);
  }
  CHECK(!slots.take(1u, base));
  CHECK_EQ(slots.taken(), GuestBytes::slot_count - 1u);
  CHECK(slots.take_at(0u, 1u)); 
  CHECK_EQ(slots.taken(), GuestBytes::slot_count); }

TEST(arena_slots_take, a_request_that_cannot_fit_anywhere_or_is_empty_changes_nothing)
{ ArenaSlots slots;
  std::uint64_t base = 7u;
  CHECK(!slots.take(0u, base));
  CHECK(!slots.take(slot * GuestBytes::slot_count, base));
  CHECK(!slots.take(~std::uint64_t{0}, base));
  CHECK_EQ(base, 7u);
  CHECK_EQ(slots.taken(), 0u); }

TEST(arena_slots_take_at, only_a_free_aligned_run_inside_the_table_is_taken)
{ ArenaSlots slots;
  CHECK(slots.take_at(0u, 4096u));
  CHECK(!slots.take_at(0u, 4096u));
  CHECK(!slots.take_at(slot / 2u, 4096u));
  CHECK(!slots.take_at(slot * GuestBytes::slot_count, 1u));
  CHECK(!slots.take_at(slot * (GuestBytes::slot_count - 1u), slot + 1u));
  CHECK(!slots.take_at(slot, 0u));
  CHECK(slots.take_at(slot * (GuestBytes::slot_count - 1u), slot));
  CHECK(!slots.take_at(slot * (GuestBytes::slot_count - 2u), 2u * slot));
  CHECK_EQ(slots.taken(), 2u); }

TEST(arena_slots_take_at, the_whole_table_can_be_taken_in_one_run)
{ ArenaSlots slots;
  CHECK(slots.take_at(0u, slot * GuestBytes::slot_count));
  CHECK_EQ(slots.taken(), GuestBytes::slot_count);
  slots.give(0u, slot * GuestBytes::slot_count);
  CHECK_EQ(slots.taken(), 0u); }

TEST(arena_slots_give, giving_back_what_was_never_taken_or_is_misaligned_frees_nothing_else)
{ ArenaSlots slots;
  std::uint64_t base = 0u;
  CHECK(slots.take(1u, base));
  slots.give(slot / 2u, 1u);
  slots.give(slot * GuestBytes::slot_count, 1u);
  slots.give(base, 0u);
  CHECK_EQ(slots.taken(), 1u);
  slots.give(base, 1u);
  CHECK_EQ(slots.taken(), 0u); }
