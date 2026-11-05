#include "harness/framework/test_registry.hpp"
#include "proto/client/blob_ring.hpp"

#include <cstdint>

using av::BlobRing;
using av::BlobRingProbe;
using av::ClientBlobLease;

namespace av {

struct BlobRingProbe {
    static void overfill(BlobRing &arena) { arena.top_ = arena.lent_ + 1u; }
    static void misplace_base(BlobRing &arena)
    {
        arena.wrapped_ = true;
        arena.lap_ = arena.base_lap_ + 1u;
        arena.base_ = arena.end_ + 1u;
    }
    static void lose_a_lap(BlobRing &arena) { arena.lap_ = arena.base_lap_ + 2u; }
    static void orphan_capacity(BlobRing &arena)
    {
        arena.block_ = nullptr;
        arena.lent_ = 1u;
    }
    static void outgrow(BlobRing &arena) { arena.peak_ring_ = arena.lent_ + 1u; }
    static void seal_behind(BlobRing &arena) { arena.sealed_ = arena.base_ - 1u; }
    static void seal_ahead(BlobRing &arena) { arena.sealed_ = arena.top_ + 1u; }
    static void seal_on_a_lost_lap(BlobRing &arena) { arena.sealed_lap_ = arena.lap_ + 1u; }
};

}

TEST(blob_ring, an_arena_with_no_block_is_never_ready)
{ BlobRing arena;
  CHECK(!arena.ready());
  ClientBlobLease lease = arena.take(16u);
  CHECK(!lease.taken());
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, a_lease_carries_room_and_an_offset)
{ std::uint8_t block[256];
  BlobRing arena(block, sizeof block);
  CHECK(arena.ready());
  ClientBlobLease first = arena.take(64u);
  CHECK(first.taken());
  CHECK(first.data() == block);
  CHECK_EQ(first.offset(), 0u);
  CHECK_EQ(first.bytes(), 64u);
  ClientBlobLease second = arena.take(32u);
  CHECK(second.data() == block + 64u);
  CHECK_EQ(second.offset(), 64u);
  CHECK_EQ(arena.used(), 96u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, a_take_past_the_end_or_of_nothing_is_denied)
{ std::uint8_t block[64];
  BlobRing arena(block, sizeof block);
  CHECK(!arena.take(65u).taken());
  CHECK(!arena.take(0u).taken());
  CHECK_EQ(arena.room(), 64u);
  CHECK(arena.take(60u).taken());
  CHECK_EQ(arena.room(), 4u);
  CHECK(!arena.take(5u).taken());
  CHECK(arena.take(4u).taken());
  CHECK_EQ(arena.room(), 0u);
  CHECK(!arena.take(1u).taken());
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, the_top_lease_comes_back_and_a_buried_one_does_not)
{ std::uint8_t block[256];
  BlobRing arena(block, sizeof block);
  ClientBlobLease first = arena.take(64u);
  ClientBlobLease second = arena.take(32u);
  arena.give_back(second);
  CHECK_EQ(arena.used(), 64u);
  ClientBlobLease third = arena.take(16u);
  arena.give_back(first);
  CHECK_EQ(arena.used(), 80u);
  CHECK(arena.invariants_hold());
  arena.give_back(third);
  CHECK_EQ(arena.used(), 64u); }


TEST(blob_ring, a_null_block_is_no_arena)
{ BlobRing arena(nullptr, 256u);
  CHECK(!arena.ready());
  CHECK(!arena.take(16u).taken());
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.room(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, an_untaken_lease_given_back_is_a_no_op)
{ std::uint8_t block[64];
  BlobRing arena(block, sizeof block);
  ClientBlobLease taken = arena.take(16u);
  arena.give_back(ClientBlobLease{});
  CHECK_EQ(arena.used(), 16u);
  static_cast<void>(taken);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, an_arena_emptied_by_the_host_lends_from_its_start_again)
{ std::uint8_t block[64];
  BlobRing arena(block, sizeof block);
  ClientBlobLease first = arena.take(16u);
  ClientBlobLease second = arena.take(32u);
  CHECK(first.taken());
  CHECK(second.taken());
  CHECK(!arena.take(17u).taken());
  arena.seal();
  arena.mark(1u);
  arena.retire_through(1u);
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.room(), 64u);
  CHECK(arena.invariants_hold());
  ClientBlobLease again = arena.take(64u);
  CHECK(again.taken());
  CHECK_EQ(again.offset(), 0u);
  CHECK(again.data() == block);
  CHECK_EQ(arena.used(), 64u);
  CHECK(arena.invariants_hold());
  arena.retire_through(1u);
  arena.retire_through(9u);
  CHECK_EQ(arena.used(), 64u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, retiring_an_arena_with_no_block_keeps_it_unready)
{ BlobRing arena;
  arena.seal();
  arena.mark(1u);
  arena.retire_through(1u);
  CHECK(!arena.ready());
  CHECK_EQ(arena.used(), 0u);
  CHECK(!arena.take(1u).taken());
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, an_unsealed_lease_outlives_every_batch_marked_before_its_seal)
{ std::uint8_t block[100];
  BlobRing arena(block, sizeof block);
  CHECK(arena.take(10u).taken());
  arena.seal();
  arena.mark(1u);
  const ClientBlobLease open = arena.take(60u);
  CHECK_EQ(open.offset(), 10u);
  arena.mark(2u);
  arena.mark(3u);
  arena.retire_through(3u);
  CHECK_EQ(arena.used(), 60u);
  CHECK(!arena.take(31u).taken());
  const ClientBlobLease below = arena.take(10u);
  CHECK(below.taken());
  CHECK_EQ(below.offset(), 70u);
  CHECK(arena.invariants_hold());
  arena.seal();
  arena.mark(4u);
  arena.retire_through(3u);
  CHECK_EQ(arena.used(), 70u);
  arena.retire_through(4u);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, a_sealed_lease_is_not_given_back)
{ std::uint8_t block[64];
  BlobRing arena(block, sizeof block);
  const ClientBlobLease framed = arena.take(16u);
  arena.seal();
  arena.give_back(framed);
  CHECK_EQ(arena.used(), 16u);
  const ClientBlobLease dropped = arena.take(8u);
  arena.give_back(dropped);
  CHECK_EQ(arena.used(), 16u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, lent_memory_replaces_the_block_and_lending_none_takes_it_away)
{ BlobRing arena;
  std::uint8_t first[32];
  arena.lend(first, sizeof first);
  CHECK(arena.ready());
  CHECK(arena.take(32u).data() == first);
  CHECK_EQ(arena.used(), 32u);
  std::uint8_t second[16];
  arena.lend(second, sizeof second);
  CHECK_EQ(arena.used(), 0u);
  CHECK(!arena.take(17u).taken());
  CHECK(arena.take(16u).data() == second);
  CHECK(arena.invariants_hold());
  arena.lend(nullptr, 64u);
  CHECK(!arena.ready());
  CHECK_EQ(arena.used(), 0u);
  CHECK(!arena.take(1u).taken());
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, the_invariant_catches_impossible_states)
{ std::uint8_t block[64];
  BlobRing overfilled(block, sizeof block);
  BlobRingProbe::overfill(overfilled);
  CHECK(!overfilled.invariants_hold());
  BlobRing orphaned(block, sizeof block);
  BlobRingProbe::orphan_capacity(orphaned);
  CHECK(!orphaned.invariants_hold());
  BlobRing outgrown(block, sizeof block);
  BlobRingProbe::outgrow(outgrown);
  CHECK(!outgrown.invariants_hold());
  std::uint8_t    other[32];
  BlobRing misplaced(other, sizeof other);
  BlobRingProbe::misplace_base(misplaced);
  CHECK(!misplaced.invariants_hold());
  BlobRing lapped(other, sizeof other);
  BlobRingProbe::lose_a_lap(lapped);
  CHECK(!lapped.invariants_hold());
  CHECK(!outgrown.invariants_hold());
  BlobRing behind(other, sizeof other);
  CHECK(behind.take(8u).taken());
  behind.seal();
  behind.mark(1u);
  CHECK(behind.take(8u).taken());
  behind.retire_through(1u);
  CHECK(behind.invariants_hold());
  BlobRingProbe::seal_behind(behind);
  CHECK(!behind.invariants_hold());
  BlobRing ahead(other, sizeof other);
  CHECK(ahead.take(8u).taken());
  BlobRingProbe::seal_ahead(ahead);
  CHECK(!ahead.invariants_hold());
  BlobRing lost(other, sizeof other);
  BlobRingProbe::seal_on_a_lost_lap(lost);
  CHECK(!lost.invariants_hold()); }

TEST(blob_ring, bytes_inside_the_block_are_held_at_their_offset)
{ std::uint8_t    memory[96];
  std::uint8_t   *block = memory + 16;
  BlobRing arena(block, 64u);
  std::uint64_t   offset = 999u;
  CHECK(arena.holds(block, 64u, offset));
  CHECK_EQ(offset, 0u);
  CHECK(arena.holds(block + 63, 1u, offset));
  CHECK_EQ(offset, 63u);
  CHECK(arena.holds(block + 40, 24u, offset));
  CHECK_EQ(offset, 40u);
  offset = 999u;
  CHECK(!arena.holds(block + 40, 25u, offset));
  CHECK(!arena.holds(block - 1, 2u, offset));
  CHECK(!arena.holds(block + 64, 1u, offset));
  CHECK(!arena.holds(block + 65, 0u, offset));
  CHECK(!arena.holds(block + 65, 1u, offset));
  CHECK(!arena.holds(block + 8, 0u, offset));
  CHECK(!arena.holds(memory, 8u, offset));
  CHECK_EQ(offset, 999u);
  BlobRing none;
  CHECK(!none.holds(block, 1u, offset));
  arena.lend(nullptr, 64u);
  CHECK(!arena.holds(block, 1u, offset));
  CHECK_EQ(offset, 999u); }

TEST(blob_ring, a_run_batch_retires_the_leases_taken_before_it)
{ std::uint8_t block[100];
  BlobRing arena(block, sizeof block);
  CHECK(arena.take(30u).taken());
  arena.seal();
  arena.mark(1u);
  CHECK(arena.take(30u).taken());
  arena.seal();
  arena.mark(2u);
  CHECK_EQ(arena.used(), 60u);
  arena.retire_through(1u);
  CHECK_EQ(arena.used(), 30u);
  arena.retire_through(1u);
  CHECK_EQ(arena.used(), 30u);
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, a_lease_wraps_to_the_start_once_the_oldest_are_retired)
{ std::uint8_t block[100];
  BlobRing arena(block, sizeof block);
  CHECK(arena.take(40u).taken());
  arena.seal();
  arena.mark(1u);
  CHECK(arena.take(40u).taken());
  arena.seal();
  arena.mark(2u);
  CHECK(!arena.take(30u).taken());
  CHECK_EQ(arena.room(), 20u);
  arena.retire_through(1u);
  CHECK_EQ(arena.room(), 40u);
  const ClientBlobLease wrapped = arena.take(30u);
  CHECK(wrapped.taken());
  CHECK_EQ(wrapped.offset(), 0u);
  CHECK(wrapped.data() == block);
  CHECK_EQ(arena.used(), 70u);
  CHECK_EQ(arena.room(), 10u);
  CHECK(!arena.take(11u).taken());
  CHECK(arena.invariants_hold());
  arena.seal();
  arena.mark(3u);
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 30u);
  CHECK(arena.invariants_hold());
  arena.retire_through(3u);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, a_mark_on_the_next_lap_retires_the_rest_of_the_last)
{ std::uint8_t block[100];
  BlobRing arena(block, sizeof block);
  CHECK(arena.take(60u).taken());
  arena.seal();
  arena.mark(1u);
  CHECK(arena.take(10u).taken());
  arena.retire_through(1u);
  CHECK(arena.take(20u).taken());
  CHECK(arena.take(50u).taken());
  arena.seal();
  arena.mark(2u);
  CHECK_EQ(arena.used(), 80u);
  CHECK(arena.invariants_hold());
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.room(), 100u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, a_batch_that_took_nothing_or_had_no_slot_joins_the_newest_mark)
{ std::uint8_t block[1024];
  BlobRing arena(block, sizeof block);
  CHECK(arena.take(10u).taken());
  arena.seal();
  arena.mark(1u);
  arena.mark(2u);
  arena.retire_through(1u);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.take(7u).taken());
  arena.seal();
  arena.mark(2u);
  arena.mark(3u);
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 0u);
  for (std::uint64_t fence = 3u; fence < 3u + BlobRing::max_marks + 2u; fence++) {
      CHECK(arena.take(1u).taken());
      arena.seal();
      arena.mark(fence);
  }
  const std::uint64_t last = 3u + BlobRing::max_marks + 1u;
  arena.retire_through(last - 2u);
  CHECK(arena.used() >= 2u);
  arena.retire_through(last);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(blob_ring, the_first_lease_of_a_lap_given_back_undoes_the_wrap)
{ std::uint8_t block[100];
  BlobRing arena(block, sizeof block);
  CHECK(arena.take(50u).taken());
  arena.seal();
  arena.mark(1u);
  CHECK(arena.take(30u).taken());
  arena.retire_through(1u);
  const ClientBlobLease wrapped = arena.take(40u);
  CHECK_EQ(wrapped.offset(), 0u);
  arena.give_back(wrapped);
  CHECK(arena.invariants_hold());
  CHECK_EQ(arena.used(), 30u);
  const ClientBlobLease above = arena.take(20u);
  CHECK_EQ(above.offset(), 80u);
  CHECK(arena.invariants_hold()); }


TEST(blob_ring, a_ring_is_idle_only_while_it_has_a_block_and_nothing_outstanding)
{ BlobRing none;
  CHECK(!none.idle());
  std::uint8_t block[64];
  BlobRing     ring(block, sizeof block);
  CHECK(ring.idle());
  CHECK(ring.take(16u).taken());
  CHECK(!ring.idle());
  ring.seal();
  ring.mark(1u);
  CHECK(!ring.idle());
  ring.retire_through(1u);
  CHECK(ring.idle());
  CHECK(ring.invariants_hold()); }

TEST(blob_ring, a_mark_with_nothing_new_keeps_the_older_fence_so_an_untouched_ring_goes_idle)
{ std::uint8_t block[64];
  BlobRing     ring(block, sizeof block);
  ring.seal();
  ring.mark(5u);
  ring.mark(9u);
  ring.mark(12u);
  ring.retire_through(5u);
  CHECK(ring.idle());
  CHECK(ring.invariants_hold()); }

TEST(blob_ring, a_placement_follows_the_takes_a_ring_would_make)
{ std::uint8_t block[100];
  BlobRing     ring(block, sizeof block);
  BlobRing::Placement plan = ring.placement();
  std::size_t at = 99u;
  bool        wraps = true;
  CHECK(!plan.place(0u, at, wraps));
  CHECK(!wraps);
  CHECK(plan.place(60u, at, wraps));
  CHECK_EQ(at, 0u);
  CHECK(!wraps);
  CHECK(!plan.place(41u, at, wraps));
  CHECK(plan.place(40u, at, wraps));
  CHECK_EQ(at, 60u);
  CHECK_EQ(ring.used(), 0u);
  CHECK(!BlobRing::Placement().place(1u, at, wraps)); }

TEST(blob_ring, a_placement_wraps_into_the_retired_start_and_stops_at_the_oldest_blob)
{ std::uint8_t block[100];
  BlobRing     ring(block, sizeof block);
  CHECK(ring.take(70u).taken());
  ring.seal();
  ring.mark(1u);
  CHECK(ring.take(20u).taken());
  ring.seal();
  ring.mark(2u);
  ring.retire_through(1u);
  BlobRing::Placement plan = ring.placement();
  std::size_t at = 0u;
  bool        wraps = false;
  CHECK(!plan.place(71u, at, wraps));
  CHECK(plan.place(30u, at, wraps));
  CHECK(wraps);
  CHECK_EQ(at, 0u);
  CHECK(plan.place(40u, at, wraps));
  CHECK(!wraps);
  CHECK_EQ(at, 30u);
  CHECK(!plan.place(1u, at, wraps));
  const ClientBlobLease taken = ring.take(30u);
  CHECK_EQ(taken.offset(), 0u);
  CHECK_EQ(ring.room(), 40u);
  CHECK(ring.invariants_hold()); }
