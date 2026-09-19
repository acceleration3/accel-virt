/* The blocks a context's buffers are carved out of its blob arena.
 *
 * A buffer backed here costs no copy to map: the device makes the host's
 * cl_mem over this same memory. So the block has to last as long as the
 * buffer - which is what makes this a suballocator with a free list rather
 * than the bump arena the generated clients lease from and retire together.
 *
 * The interesting cases are the ones a long-running application reaches: a
 * block given back is reusable, a gap between two live blocks is filled
 * rather than skipped, and an arena with no room refuses instead of handing
 * out an offset that overlaps something live. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"

#include <cstdint>

using av::clicd::ClArenaBlocks;

namespace {

alignas(4096) std::uint8_t arena[64u * 4096u];

}  // namespace

TEST(cl_arena_blocks, lends_nothing_until_it_is_given_an_arena)
{ ClArenaBlocks blocks;
  std::uint64_t offset = 0xEEu;
  CHECK(!blocks.ready());
  /* No arena: every buffer is the staged path, and nothing is handed out. */
  CHECK(!blocks.take(4096u, offset));
  CHECK_EQ(offset, 0xEEu);
  CHECK(blocks.at(0u, 1u) == nullptr);
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, a_block_is_the_arena_s_own_memory_at_the_offset_it_answered)
{ ClArenaBlocks blocks;
  blocks.lend(arena, sizeof arena);
  CHECK(blocks.ready());
  std::uint64_t first = 0xEEu;
  CHECK(blocks.take(128u, first));
  /* The first block sits at the start of the arena: the device adds the
   * offset to the blob's host address, so the two views must agree. */
  CHECK_EQ(first, 0u);
  CHECK(blocks.at(first, 128u) == arena);
  CHECK_EQ(blocks.live(), 1u);

  std::uint64_t second = 0u;
  CHECK(blocks.take(128u, second));
  /* Page aligned, because a host that takes CL_MEM_USE_HOST_PTR wants its
   * own base alignment and pages are the coarsest anything here asks. */
  CHECK_EQ(second, 4096u);
  CHECK(blocks.at(second, 128u) == arena + 4096u);
  /* Two live blocks never overlap, which is the invariant that matters. */
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, reading_past_a_block_or_outside_every_block_answers_nothing)
{ ClArenaBlocks blocks;
  blocks.lend(arena, sizeof arena);
  std::uint64_t taken = 0u;
  CHECK(blocks.take(128u, taken));
  CHECK(blocks.at(taken, 128u) == arena);
  /* Past the block it was given, even though the arena has the room. */
  CHECK(blocks.at(taken, 4097u) == nullptr);
  /* An offset no block was handed out at. */
  CHECK(blocks.at(8192u, 1u) == nullptr);
  /* And arithmetic that would wrap. */
  CHECK(blocks.at(~std::uint64_t{0}, 1u) == nullptr);
  CHECK(blocks.at(taken, ~std::uint64_t{0}) == nullptr);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, a_block_given_back_is_handed_out_again)
{ ClArenaBlocks blocks;
  blocks.lend(arena, sizeof arena);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK(blocks.take(128u, first));
  CHECK(blocks.take(128u, second));
  CHECK_EQ(blocks.live(), 2u);

  blocks.give_back(first);
  CHECK_EQ(blocks.live(), 1u);
  /* Forgotten, so it names nothing until it is handed out again. */
  CHECK(blocks.at(first, 128u) == nullptr);
  CHECK(blocks.invariants_hold());

  /* The gap the first left is filled rather than skipped: an application
   * that makes and releases buffers in a loop must not walk off the end. */
  std::uint64_t again = 0xEEu;
  CHECK(blocks.take(128u, again));
  CHECK_EQ(again, first);
  CHECK(blocks.at(again, 128u) == arena);
  CHECK_EQ(blocks.live(), 2u);
  /* The one still live was never disturbed. */
  CHECK(blocks.at(second, 128u) == arena + 4096u);
  CHECK(blocks.invariants_hold());

  /* Giving back an offset no block holds changes nothing. */
  blocks.give_back(0x9000u);
  blocks.give_back(0xEEEEu);
  CHECK_EQ(blocks.live(), 2u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, an_arena_with_no_room_refuses_rather_than_overlapping_what_is_live)
{ ClArenaBlocks blocks;
  /* Two pages, so the third block of a page has nowhere to go. */
  blocks.lend(arena, 2u * 4096u);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  std::uint64_t third = 0xEEu;
  CHECK(blocks.take(4096u, first));
  CHECK(blocks.take(4096u, second));
  CHECK(!blocks.take(1u, third));
  CHECK_EQ(third, 0xEEu);
  CHECK_EQ(blocks.live(), 2u);
  /* The caller stages that buffer instead, which is slower and never wrong. */
  CHECK(blocks.invariants_hold());

  /* A block larger than the whole arena is refused whatever is live. */
  ClArenaBlocks empty;
  empty.lend(arena, 2u * 4096u);
  std::uint64_t huge = 0xEEu;
  CHECK(!empty.take(3u * 4096u, huge));
  CHECK(!empty.take(~std::uint64_t{0}, huge));
  CHECK_EQ(huge, 0xEEu);
  CHECK_EQ(empty.live(), 0u);
  CHECK(empty.invariants_hold()); }

TEST(cl_arena_blocks, a_block_of_nothing_is_not_a_block)
{ ClArenaBlocks blocks;
  blocks.lend(arena, sizeof arena);
  std::uint64_t offset = 0xEEu;
  CHECK(!blocks.take(0u, offset));
  CHECK_EQ(offset, 0xEEu);
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, the_table_being_full_refuses_rather_than_forgetting_one)
{ ClArenaBlocks blocks;
  blocks.lend(arena, sizeof arena);
  std::uint64_t offset = 0u;
  for (std::uint32_t index = 0u; index < ClArenaBlocks::max_blocks; ++index)
      CHECK(blocks.take(16u, offset));
  CHECK_EQ(blocks.live(), ClArenaBlocks::max_blocks);
  std::uint64_t one_more = 0xEEu;
  CHECK(!blocks.take(16u, one_more));
  CHECK_EQ(one_more, 0xEEu);
  CHECK_EQ(blocks.live(), ClArenaBlocks::max_blocks);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, a_new_arena_forgets_every_block_of_the_old_one)
{ ClArenaBlocks blocks;
  blocks.lend(arena, sizeof arena);
  std::uint64_t taken = 0u;
  CHECK(blocks.take(128u, taken));
  CHECK_EQ(blocks.live(), 1u);
  /* The context closed its arena. A block of memory that is gone must not
   * still name an address. */
  blocks.lend(nullptr, 0u);
  CHECK(!blocks.ready());
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.at(taken, 128u) == nullptr);
  CHECK(blocks.invariants_hold()); }
