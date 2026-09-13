/* The block one command decodes into. What matters to a caller: a decode that
 * fits leaves the count alone, one that does not is counted, and the next
 * command starts from the top. */
#include "device/executor/decode_arena.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::DecodeArena;

namespace {

/* Small on purpose: what is being tested is the refusal, and a block the size
 * the library actually gives a session would make that a slow test. */
constexpr std::size_t block_bytes = 4096u;

}  // namespace

TEST(decode_arena, a_decode_that_fits_costs_no_refusal)
{ std::uint8_t block[block_bytes];
  DecodeArena arena;
  arena.attach(block, sizeof block);
  CHECK_EQ(arena.refusals(), 0u);
  CHECK(arena.arena().allocate<std::uint8_t>(1024u) != nullptr);
  CHECK_EQ(arena.arena().used(), 1024u);
  CHECK_EQ(arena.refusals(), 0u);
  CHECK(arena.invariants_hold()); }

/* A count off the wire is a count an attacker chose; the block refuses and
 * says it refused rather than serving less than was asked for. */
TEST(decode_arena, a_command_larger_than_the_block_is_refused_and_counted)
{ std::uint8_t block[block_bytes];
  DecodeArena arena;
  arena.attach(block, sizeof block);
  CHECK(arena.arena().allocate<std::uint8_t>(block_bytes + 1u) == nullptr);
  CHECK_EQ(arena.refusals(), 1u);
  CHECK_EQ(arena.arena().used(), 0u);
  CHECK(arena.invariants_hold()); }

TEST(decode_arena, the_next_command_starts_from_the_top)
{ std::uint8_t block[block_bytes];
  DecodeArena arena;
  arena.attach(block, sizeof block);
  CHECK(arena.arena().allocate<std::uint8_t>(block_bytes) != nullptr);
  CHECK(arena.arena().allocate<std::uint8_t>(1u) == nullptr);
  arena.reset();
  CHECK_EQ(arena.arena().used(), 0u);
  CHECK(arena.arena().allocate<std::uint8_t>(block_bytes) != nullptr);
  /* The count is the arena's life, not the command's: it survives the reset,
   * because it is what says the block is the wrong size. */
  CHECK_EQ(arena.refusals(), 1u);
  CHECK(arena.invariants_hold()); }

/* An arena nobody gave a block to refuses everything, which is what an unbound
 * session must answer rather than crashing. */
TEST(decode_arena, with_no_block_it_refuses_everything)
{ DecodeArena arena;
  CHECK_EQ(arena.capacity(), 0u);
  CHECK(arena.arena().allocate<std::uint8_t>(1u) == nullptr);
  CHECK_EQ(arena.refusals(), 1u);
  CHECK(arena.invariants_hold()); }

/* A session slot is reused, and the library gives it a megabyte apiece. */
TEST(decode_arena, the_library_gives_a_session_the_whole_policy)
{ std::uint8_t block[block_bytes];
  DecodeArena arena;
  arena.attach(block, sizeof block);
  CHECK_EQ(arena.capacity(), block_bytes);
  CHECK(DecodeArena::arena_bytes > block_bytes);
  CHECK(arena.invariants_hold()); }
