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
    static void overfill(ClientBlobArena &arena) { arena.used_ = arena.capacity_ + 1u; }
    static void orphan_capacity(ClientBlobArena &arena)
    {
        arena.block_ = nullptr;
        arena.capacity_ = 1u;
    }
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
  CHECK(!orphaned.invariants_hold()); }
