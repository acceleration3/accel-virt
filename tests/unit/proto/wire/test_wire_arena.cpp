/* Where a decoded command's arrays and chained structs go. The guest sends a
 * frame of bytes; what the host hands its backend is C structures pointing at
 * each other, and every one of those pointers has to point somewhere the
 * caller owns and can release at once (feature 063). */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/wire/wire_arena.hpp"

#include <cstdint>
#include <cstring>

using av::Status;
using av::WireArena;

namespace {

struct Pair {
    std::uint32_t first;
    std::uint32_t second;
};

}  // namespace

TEST(wire_arena, hands_out_aligned_room_from_the_block_it_was_given)
{ alignas(16) std::uint8_t block[256];
  WireArena arena(block, sizeof block);
  Pair *pair = arena.allocate<Pair>(1u);
  CHECK(pair != nullptr);
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(pair) % alignof(Pair), 0u);
  pair->first = 7u;
  std::uint8_t *bytes = arena.allocate<std::uint8_t>(1u);
  Pair *after = arena.allocate<Pair>(2u);
  CHECK(bytes != nullptr);
  CHECK(after != nullptr);
  /* The one byte does not leave the next allocation misaligned. */
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(after) % alignof(Pair), 0u);
  CHECK_EQ(pair->first, 7u);
  CHECK(arena.invariants_hold()); }

/* A frame that asks for more than the block holds is a frame that gets
 * nothing: the decoder sees a null pointer and says malformed, rather than the
 * host writing past what it owns. */
TEST(wire_arena, refuses_what_the_block_cannot_hold)
{ std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  CHECK(arena.allocate<std::uint8_t>(65u) == nullptr);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.allocate<std::uint8_t>(64u) != nullptr);
  CHECK(arena.allocate<std::uint8_t>(1u) == nullptr);
  CHECK(arena.invariants_hold()); }

/* A count that overflows when multiplied by the element size is the oldest
 * trick there is, and it arrives from outside. */
TEST(wire_arena, refuses_a_count_that_overflows)
{ std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  CHECK(arena.allocate<Pair>(~std::size_t{0}) == nullptr);
  CHECK(arena.allocate<Pair>(~std::size_t{0} / 4u) == nullptr);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(wire_arena, an_empty_block_hands_out_nothing)
{ WireArena none(nullptr, 128u);
  CHECK(none.allocate<std::uint8_t>(1u) == nullptr);
  std::uint8_t block[8];
  WireArena empty(block, 0u);
  CHECK(empty.allocate<std::uint8_t>(1u) == nullptr);
  CHECK(none.invariants_hold());
  CHECK(empty.invariants_hold()); }

/* Zero of something is not a failure, and must not be a pointer into the next
 * allocation's room either: a decoder that reads a zero-length array asks for
 * zero and uses nothing. */
TEST(wire_arena, zero_of_something_is_nothing)
{ std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  CHECK(arena.allocate<Pair>(0u) == nullptr);
  CHECK_EQ(arena.used(), 0u); }

/* One frame's decode is one arena's life: the host resets it and the next
 * frame starts from the top of the same block. */
TEST(wire_arena_reset, gives_the_whole_block_back)
{ std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  CHECK(arena.allocate<std::uint8_t>(64u) != nullptr);
  CHECK_EQ(arena.used(), 64u);
  arena.reset();
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.allocate<std::uint8_t>(64u) != nullptr);
  CHECK(arena.invariants_hold()); }

TEST(wire_arena, characters_come_from_the_same_block)
{ std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  char *text = arena.allocate_chars(5u);
  CHECK(text != nullptr);
  std::memcpy(text, "abcd", 5u);
  CHECK_EQ(arena.used(), 5u);
  CHECK(std::strcmp(text, "abcd") == 0); }

/* A decoder reads part of what it is about to write - the generated pNext
 * codec walks the destination's chain before filling it - so what the arena
 * hands out has to start at zero rather than at whatever the last frame left. */
TEST(wire_arena, what_it_hands_out_is_zeroed)
{ std::uint8_t block[64];
  std::memset(block, 0xAB, sizeof block);
  WireArena arena(block, sizeof block);
  auto *const first = arena.allocate<std::uint64_t>(2u);
  CHECK(first != nullptr);
  CHECK_EQ(first[0], 0u);
  CHECK_EQ(first[1], 0u);
  first[0] = 0x1122334455667788ull;
  arena.reset();
  auto *const again = arena.allocate<std::uint64_t>(2u);
  CHECK_EQ(again[0], 0u);
  CHECK(arena.invariants_hold()); }

/* A block whose end is not a multiple of what is asked for next: the padding
 * alone runs past the block, and no allocation may start outside it. */
TEST(wire_arena, padding_that_runs_past_the_block_allocates_nothing)
{ alignas(8) std::uint8_t block[8];
  WireArena arena(block, 5u);
  CHECK(arena.allocate<std::uint8_t>(5u) != nullptr);
  CHECK_EQ(arena.used(), 5u);
  CHECK(arena.allocate<std::uint32_t>() == nullptr);
  CHECK_EQ(arena.used(), 5u);
  CHECK(arena.invariants_hold()); }

/* A host has to tell "the frame was malformed" from "this block was too small
 * for it": the guest is told the same thing either way, but an operator who
 * cannot tell them apart cannot size the block. */
TEST(wire_arena, counts_what_it_would_not_give_room_for)
{ std::uint8_t block[16];
  WireArena arena(block, sizeof block);
  CHECK_EQ(arena.refusals(), 0u);
  CHECK(arena.allocate<std::uint8_t>(64u) == nullptr);
  CHECK_EQ(arena.refusals(), 1u);
  CHECK(arena.allocate<std::uint8_t>(0u) == nullptr);
  CHECK(arena.allocate<std::uint64_t>(~std::size_t{0}) == nullptr);
  CHECK_EQ(arena.refusals(), 3u);
  /* What it did give room for is not a refusal. */
  CHECK(arena.allocate<std::uint8_t>(16u) != nullptr);
  CHECK_EQ(arena.refusals(), 3u);
  /* And a reset starts the next decode, not the count. */
  arena.reset();
  CHECK_EQ(arena.refusals(), 3u);
  CHECK(arena.invariants_hold()); }

/* One arena serves one context, and a context is a slot the device reuses. The
 * block changes; what it refused over its life does not. */
TEST(wire_arena, takes_a_different_block_and_keeps_the_count)
{ std::uint8_t first[16];
  std::uint8_t second[64];
  WireArena arena(first, sizeof first);
  CHECK(arena.allocate<std::uint8_t>(32u) == nullptr);
  CHECK_EQ(arena.refusals(), 1u);
  CHECK(arena.allocate<std::uint8_t>(16u) != nullptr);

  arena.attach(second, sizeof second);
  CHECK_EQ(arena.capacity(), 64u);
  CHECK_EQ(arena.used(), 0u);
  CHECK_EQ(arena.refusals(), 1u);
  CHECK(arena.allocate<std::uint8_t>(32u) != nullptr);
  CHECK(arena.invariants_hold()); }

/* No block is an arena that refuses everything, which is what an unbound one
 * has to answer. */
TEST(wire_arena, no_block_refuses_everything)
{ std::uint8_t block[16];
  WireArena arena(block, sizeof block);
  arena.attach(nullptr, 64u);
  CHECK_EQ(arena.capacity(), 0u);
  CHECK(arena.allocate<std::uint8_t>(1u) == nullptr);
  CHECK(arena.invariants_hold()); }

/* Docs/protocol.md, "Long commands": a command longer than the block decodes
 * into chunks from the heap past it, every one aligned and zeroed, and a
 * reset gives them all back. */
TEST(wire_arena, overflows_past_its_block_into_chunks_reset_gives_back)
{ av::MockPlatform platform;
  alignas(16) std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  arena.overflow_into(platform);
  std::uint8_t *const first = arena.allocate<std::uint8_t>(40u);
  CHECK(first != nullptr);
  CHECK_EQ(arena.chunks(), 0u);
  /* Past the block: a chunk of its own. */
  Pair *const pairs = arena.allocate<Pair>(1000u);
  CHECK(pairs != nullptr);
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(pairs) % alignof(Pair), 0u);
  CHECK_EQ(pairs[999u].second, 0u);
  pairs[999u].second = 5u;
  CHECK_EQ(arena.chunks(), 1u);
  CHECK_EQ(arena.used(), 40u + 1000u * sizeof(Pair));
  /* Small ones follow in the chunk's own room, then in another. */
  CHECK(arena.allocate<std::uint8_t>(3u) != nullptr);
  CHECK_EQ(arena.chunks(), 1u);
  CHECK(arena.allocate<std::uint8_t>(std::size_t{1} << 21) != nullptr);
  CHECK_EQ(arena.chunks(), 2u);
  CHECK(arena.invariants_hold());
  CHECK_EQ(arena.refusals(), 0u);
  arena.reset();
  CHECK_EQ(arena.chunks(), 0u);
  CHECK_EQ(arena.used(), 0u);
  CHECK(arena.allocate<std::uint8_t>(64u) == block);
  CHECK(arena.invariants_hold()); }

/* Without the heap's memory it refuses, and counts it, as without anywhere
 * to overflow; attach gives chunks back too. */
TEST(wire_arena, an_overflow_the_heap_refuses_is_a_refusal)
{ av::MockPlatform platform;
  std::uint8_t block[16];
  WireArena arena(block, sizeof block);
  arena.overflow_into(platform);
  platform.fail_allocation_at(1u);
  CHECK(arena.allocate<std::uint8_t>(17u) == nullptr);
  CHECK_EQ(arena.refusals(), 1u);
  CHECK(arena.allocate<std::uint8_t>(17u) != nullptr);
  CHECK_EQ(arena.chunks(), 1u);
  arena.attach(block, sizeof block);
  CHECK_EQ(arena.chunks(), 0u);
  CHECK(arena.invariants_hold()); }
