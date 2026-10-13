/* Room a large array is lent to cross as a blob. What matters to the OpenGL
 * client: a lease is handed out only from a ready arena with room, a lease
 * carries somewhere to copy to and an offset the host reads back, and the top
 * lease's room comes back when its command overflowed before it was sent. */
#include "harness/framework/test_registry.hpp"
#include "proto/client/client_blob_arena.hpp"

#include <cstdint>

using av::ClientBlobArena;
using av::ClientBlobArenaProbe;
using av::ClientBlobLease;

namespace av {

struct ClientBlobArenaProbe {
    /* States the class never builds: used past capacity, and a block that is
     * gone while a capacity remains - both what the invariant is there to catch. */
    static void overfill(ClientBlobArena &arena) { arena.top_ = arena.capacity_ + 1u; }
    /* Wrapped with the oldest lease above the end of the lap, and on a lap
     * the base never left. */
    static void misplace_base(ClientBlobArena &arena)
    {
        arena.wrapped_ = true;
        arena.lap_ = arena.base_lap_ + 1u;
        arena.base_ = arena.end_ + 1u;
    }
    static void lose_a_lap(ClientBlobArena &arena) { arena.lap_ = arena.base_lap_ + 2u; }
    static void orphan_capacity(ClientBlobArena &arena)
    {
        arena.block_ = nullptr;
        arena.capacity_ = 1u;
    }
    static void outgrow(ClientBlobArena &arena) { arena.capacity_ = arena.lent_ + 1u; }
};

}  // namespace av

TEST(client_blob_arena, an_arena_with_no_block_is_never_ready)
{ ClientBlobArena arena;
  CHECK(!arena.ready());
  ClientBlobLease lease = arena.take(16u);
  CHECK(!lease.taken());
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, a_lease_carries_room_and_an_offset)
{ std::uint8_t block[256];
  ClientBlobArena arena(block, sizeof block);
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

TEST(client_blob_arena, a_take_past_the_end_or_of_nothing_is_denied)
{ std::uint8_t block[64];
  ClientBlobArena arena(block, sizeof block);
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

TEST(client_blob_arena, the_top_lease_comes_back_and_a_buried_one_does_not)
{ std::uint8_t block[256];
  ClientBlobArena arena(block, sizeof block);
  ClientBlobLease first = arena.take(64u);
  ClientBlobLease second = arena.take(32u);
  arena.give_back(second);
  CHECK_EQ(arena.used(), 64u);
  /* first is no longer the top only because second was given back; giving back
   * a lease that is not the top changes nothing. */
  ClientBlobLease third = arena.take(16u);
  arena.give_back(first);
  CHECK_EQ(arena.used(), 80u);
  CHECK(arena.invariants_hold());
  /* Giving back the current top does free it. */
  arena.give_back(third);
  CHECK_EQ(arena.used(), 64u); }


/* A null block, whatever the capacity says, is no arena: not ready, and it
 * lends nothing. */
TEST(client_blob_arena, a_null_block_is_no_arena)
{ ClientBlobArena arena(nullptr, 256u);
  CHECK(!arena.ready());
  CHECK(!arena.take(16u).taken());
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.room(), 0u);
  CHECK(arena.invariants_hold()); }

/* Giving back a lease that was never taken changes nothing. */
TEST(client_blob_arena, an_untaken_lease_given_back_is_a_no_op)
{ std::uint8_t block[64];
  ClientBlobArena arena(block, sizeof block);
  ClientBlobLease taken = arena.take(16u);
  arena.give_back(ClientBlobLease{});
  CHECK_EQ(arena.used(), 16u);
  static_cast<void>(taken);
  CHECK(arena.invariants_hold()); }

/* Every lease retires at once when an answer shows the host has read them
 * all: the next take starts at the beginning again, and the room is whole. */
TEST(client_blob_arena, retiring_returns_every_lease_and_the_next_take_starts_at_zero)
{ std::uint8_t block[64];
  ClientBlobArena arena(block, sizeof block);
  ClientBlobLease first = arena.take(16u);
  ClientBlobLease second = arena.take(48u);
  CHECK(first.taken());
  CHECK(second.taken());
  CHECK(!arena.take(1u).taken());
  arena.retire_all();
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold());
  ClientBlobLease again = arena.take(64u);
  CHECK(again.taken());
  CHECK_EQ(again.offset(), 0u);
  CHECK(again.data() == block);
  CHECK_EQ(arena.used(), 64u);
  CHECK(arena.invariants_hold());
  /* A retire with nothing lent leaves it empty. */
  arena.retire_all();
  arena.retire_all();
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(client_blob_arena, retiring_an_arena_with_no_block_keeps_it_unready)
{ ClientBlobArena arena;
  arena.retire_all();
  CHECK(!arena.ready());
  CHECK_EQ(arena.used(), 0u);
  CHECK(!arena.take(1u).taken());
  CHECK(arena.invariants_hold()); }

/* Memory lent later - a GL context's chunk once it is imported - replaces the
 * arena's block with nothing taken from it; lending nothing takes it away. */
TEST(client_blob_arena, lent_memory_replaces_the_block_and_lending_none_takes_it_away)
{ ClientBlobArena arena;
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

/* Room set aside comes off the top, is never lent again - a retire included
 * - and goes back only in the order it was taken, since lending grows at the
 * top. */
TEST(client_blob_arena, room_set_aside_comes_off_the_top_and_goes_back_last_first)
{ std::uint8_t    block[64];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.take(8u).taken());
  CHECK(!arena.set_aside(0u).taken());
  CHECK(!arena.set_aside(57u).taken());
  const av::ClientBlobLease frames = arena.set_aside(32u);
  CHECK(frames.taken());
  CHECK(frames.data() == block + 32);
  CHECK_EQ(frames.offset(), 32u);
  CHECK_EQ(frames.bytes(), 32u);
  const av::ClientBlobLease more = arena.set_aside(16u);
  CHECK_EQ(more.offset(), 16u);
  CHECK_EQ(arena.room(), 8u);
  arena.retire_all();
  CHECK_EQ(arena.room(), 16u);
  CHECK(!arena.take(17u).taken());
  CHECK(arena.invariants_hold());
  /* Not the most recent, nothing taken, or taken twice: kept aside. */
  arena.restore(frames);
  arena.restore(av::ClientBlobLease{});
  CHECK_EQ(arena.room(), 16u);
  arena.restore(more);
  CHECK_EQ(arena.room(), 32u);
  arena.restore(more);
  CHECK_EQ(arena.room(), 32u);
  arena.restore(frames);
  CHECK_EQ(arena.room(), 64u);
  CHECK(arena.invariants_hold());
  ClientBlobArena none;
  CHECK(!none.set_aside(1u).taken()); }

/* The invariant catches the states the class never builds. */
TEST(client_blob_arena, the_invariant_catches_impossible_states)
{ std::uint8_t block[64];
  ClientBlobArena overfilled(block, sizeof block);
  ClientBlobArenaProbe::overfill(overfilled);
  CHECK(!overfilled.invariants_hold());
  ClientBlobArena orphaned(block, sizeof block);
  ClientBlobArenaProbe::orphan_capacity(orphaned);
  CHECK(!orphaned.invariants_hold());
  ClientBlobArena outgrown(block, sizeof block);
  ClientBlobArenaProbe::outgrow(outgrown);
  CHECK(!outgrown.invariants_hold());
  std::uint8_t    other[32];
  ClientBlobArena misplaced(other, sizeof other);
  ClientBlobArenaProbe::misplace_base(misplaced);
  CHECK(!misplaced.invariants_hold());
  ClientBlobArena lapped(other, sizeof other);
  ClientBlobArenaProbe::lose_a_lap(lapped);
  CHECK(!lapped.invariants_hold());
  CHECK(!outgrown.invariants_hold()); }

/* Bytes already inside the block cross by offset, with no lease: the first
 * and last byte of it, a span ending on its last byte, and room set aside -
 * which the host sees as well as any. A span that starts before it, starts
 * past it or runs off its end is the application's own memory, and so is any
 * span at all with no arena, or one of no bytes. */
TEST(client_blob_arena, bytes_inside_the_block_are_held_at_their_offset)
{ std::uint8_t    memory[96];
  std::uint8_t   *block = memory + 16;
  ClientBlobArena arena(block, 64u);
  std::uint64_t   offset = 999u;
  CHECK(arena.holds(block, 64u, offset));
  CHECK_EQ(offset, 0u);
  CHECK(arena.holds(block + 63, 1u, offset));
  CHECK_EQ(offset, 63u);
  CHECK(arena.holds(block + 40, 24u, offset));
  CHECK_EQ(offset, 40u);
  const ClientBlobLease aside = arena.set_aside(16u);
  CHECK(arena.holds(aside.data(), 16u, offset));
  CHECK_EQ(offset, 48u);
  offset = 999u;
  CHECK(!arena.holds(block + 40, 25u, offset));
  CHECK(!arena.holds(block - 1, 2u, offset));
  CHECK(!arena.holds(block + 64, 1u, offset));
  CHECK(!arena.holds(block + 65, 0u, offset));
  CHECK(!arena.holds(block + 65, 1u, offset));
  CHECK(!arena.holds(block + 8, 0u, offset));
  CHECK(!arena.holds(memory, 8u, offset));
  CHECK_EQ(offset, 999u);
  ClientBlobArena none;
  CHECK(!none.holds(block, 1u, offset));
  arena.lend(nullptr, 64u);
  CHECK(!arena.holds(block, 1u, offset));
  CHECK_EQ(offset, 999u); }

/* A batch the host has run gives back the leases taken before it was
 * published, oldest first, while later ones stay lent. */
TEST(client_blob_arena, a_run_batch_retires_the_leases_taken_before_it)
{ std::uint8_t block[100];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.take(30u).taken());
  arena.mark(1u);
  CHECK(arena.take(30u).taken());
  arena.mark(2u);
  CHECK_EQ(arena.used(), 60u);
  arena.retire_through(1u);
  CHECK_EQ(arena.used(), 30u);
  arena.retire_through(1u);   /* again: nothing more */
  CHECK_EQ(arena.used(), 30u);
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

/* Past the end, a lease starts over at the block's start once the oldest
 * leases there are retired - and not before. */
TEST(client_blob_arena, a_lease_wraps_to_the_start_once_the_oldest_are_retired)
{ std::uint8_t block[100];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.take(40u).taken());
  arena.mark(1u);
  CHECK(arena.take(40u).taken());
  arena.mark(2u);
  CHECK(!arena.take(30u).taken());   /* 20 above, and nothing retired below */
  CHECK_EQ(arena.room(), 20u);
  arena.retire_through(1u);
  CHECK_EQ(arena.room(), 40u);       /* the start, freed */
  const ClientBlobLease wrapped = arena.take(30u);
  CHECK(wrapped.taken());
  CHECK_EQ(wrapped.offset(), 0u);
  CHECK(wrapped.data() == block);
  CHECK_EQ(arena.used(), 70u);
  CHECK_EQ(arena.room(), 10u);       /* up to the oldest */
  CHECK(!arena.take(11u).taken());
  CHECK(arena.invariants_hold());
  arena.mark(3u);
  /* The old lap's last batch, then the new lap's. */
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 30u);
  CHECK(arena.invariants_hold());
  arena.retire_through(3u);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

/* A mark on the next lap retires whatever is left of the one before. */
TEST(client_blob_arena, a_mark_on_the_next_lap_retires_the_rest_of_the_last)
{ std::uint8_t block[100];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.take(60u).taken());
  arena.mark(1u);
  arena.retire_through(1u);
  CHECK(arena.take(30u).taken());     /* 60..90 */
  CHECK(arena.take(50u).taken());     /* wraps: 0..50 */
  arena.mark(2u);
  CHECK_EQ(arena.used(), 80u);
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.room(), 50u);        /* 50..100 */
  CHECK(arena.invariants_hold()); }

/* A batch that took nothing, or one past the marks there is room for, is
 * covered by the newest mark: its leases are retired no sooner than its own. */
TEST(client_blob_arena, a_batch_that_took_nothing_or_had_no_slot_joins_the_newest_mark)
{ std::uint8_t block[1024];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.take(10u).taken());
  arena.mark(1u);
  arena.mark(2u);                      /* nothing taken since */
  arena.retire_through(1u);
  CHECK_EQ(arena.used(), 10u);         /* the mark now waits for 2 */
  arena.retire_through(2u);
  CHECK_EQ(arena.used(), 0u);
  for (std::uint64_t fence = 3u; fence < 3u + ClientBlobArena::max_marks + 2u; fence++) {
      CHECK(arena.take(1u).taken());
      arena.mark(fence);
  }
  const std::uint64_t last = 3u + ClientBlobArena::max_marks + 1u;
  arena.retire_through(last - 2u);     /* the merged mark is newer than this */
  CHECK(arena.used() >= 2u);
  arena.retire_through(last);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

/* The first lease of a lap, given back, takes the lap back: the next take
 * goes where it would have before the wrap. */
TEST(client_blob_arena, the_first_lease_of_a_lap_given_back_undoes_the_wrap)
{ std::uint8_t block[100];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.take(50u).taken());
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

/* Room is set aside off a top no lease lives in, wrapped or not. */
TEST(client_blob_arena, room_is_not_set_aside_where_a_lease_still_lives)
{ std::uint8_t block[100];
  ClientBlobArena arena(block, sizeof block);
  CHECK(arena.take(70u).taken());
  arena.mark(1u);
  CHECK(!arena.set_aside(40u).taken());
  CHECK(arena.take(10u).taken());      /* 70..80 */
  arena.mark(2u);
  arena.retire_through(1u);
  CHECK(arena.take(25u).taken());      /* 20 left above: wraps to 0..25, the lap ending at 80 */
  CHECK(!arena.set_aside(21u).taken());
  const ClientBlobLease aside = arena.set_aside(20u);
  CHECK(aside.taken());
  CHECK_EQ(aside.offset(), 80u);
  CHECK(arena.invariants_hold());
  arena.restore(aside);
  CHECK(arena.invariants_hold()); }
