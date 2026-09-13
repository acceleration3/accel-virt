/* Room a large array is lent to cross as a blob. What matters to the OpenGL
 * client: a lease is handed out only from a ready arena with room, a lease
 * carries somewhere to copy to and an offset the host reads back, and the top
 * lease's room comes back when its command overflowed before it was sent. */
#include "harness/framework/test_registry.hpp"
#include "proto/client_blob_arena.hpp"

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
  CHECK(arena.take(64u).taken());
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

/* The invariant catches the states the class never builds. */
TEST(client_blob_arena, the_invariant_catches_impossible_states)
{ std::uint8_t block[64];
  ClientBlobArena overfilled(block, sizeof block);
  ClientBlobArenaProbe::overfill(overfilled);
  CHECK(!overfilled.invariants_hold());
  ClientBlobArena orphaned(block, sizeof block);
  ClientBlobArenaProbe::orphan_capacity(orphaned);
  CHECK(!orphaned.invariants_hold()); }
