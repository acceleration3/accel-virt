/* The block of a context's arena one transfer stages its bytes in. What a
 * transfer depends on: a block is all of what it says it is and lies inside
 * the arena, it is the whole transfer when the arena has room and as much as
 * the arena can spare when it does not, it is never smaller than worth
 * staging, and it goes back when the transfer is done - whatever happened. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"
#include "icd/opencl/cl_staging_block.hpp"

#include <cstdint>

using av::clicd::ClArenaBlocks;
using av::clicd::ClStagingBlock;

namespace {

alignas(4096) std::uint8_t arena[8u << 20];

av::LockTrace           trace;
av::RecordingThreadLock lock(trace, 'B');

}  // namespace

TEST(cl_staging_block, a_transfer_the_arena_has_room_for_is_staged_whole_in_pages)
{ ClArenaBlocks blocks(lock);
  blocks.lend(arena, sizeof arena);
  { ClStagingBlock staging(blocks);
    CHECK(staging.take((3u << 20) + 1u));
    CHECK_EQ(staging.bytes(), (3u << 20) + 4096u);
    CHECK(staging.at() == arena);
    CHECK_EQ(blocks.live(), 1u);
    /* One block per transfer: a second take is refused and changes nothing. */
    CHECK(!staging.take(ClStagingBlock::least));
    CHECK_EQ(staging.bytes(), (3u << 20) + 4096u);
    CHECK_EQ(blocks.live(), 1u); }
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.invariants_hold()); }

/* More than `most` is `most`: the arena backs buffers too, and one transfer
 * must leave them room. */
TEST(cl_staging_block, a_transfer_larger_than_most_takes_most)
{ ClArenaBlocks blocks(lock);
  blocks.lend(arena, sizeof arena);
  ClStagingBlock staging(blocks);
  CHECK(staging.take(ClStagingBlock::most + 1u));
  CHECK_EQ(staging.bytes(), ClStagingBlock::most);
  ClStagingBlock exactly(blocks);
  CHECK(exactly.take(ClStagingBlock::most));
  CHECK_EQ(exactly.bytes(), ClStagingBlock::most);
  CHECK(exactly.at() == arena + ClStagingBlock::most); }

/* A crowded arena lends what it can, halving the ask, and never less than
 * `least`. */
TEST(cl_staging_block, a_crowded_arena_lends_half_and_half_again_down_to_least)
{ ClArenaBlocks blocks(lock);
  blocks.lend(arena, 1u << 20);
  { ClStagingBlock staging(blocks);
    CHECK(staging.take(3u << 20));
    CHECK_EQ(staging.bytes(), 768u << 10); }
  { ClStagingBlock staging(blocks);
    CHECK(staging.take(1536u << 10));
    CHECK_EQ(staging.bytes(), 768u << 10); }
  std::uint64_t held = 0u;
  CHECK(blocks.take((1u << 20) - ClStagingBlock::least, held));
  { ClStagingBlock staging(blocks);
    CHECK(staging.take(1u << 20));
    CHECK_EQ(staging.bytes(), ClStagingBlock::least); }
  std::uint64_t last = 0u;
  CHECK(blocks.take(4096u, last));
  { ClStagingBlock staging(blocks);
    CHECK(!staging.take(1u << 20));
    CHECK(staging.at() == nullptr);
    CHECK_EQ(staging.bytes(), 0u); }
  CHECK_EQ(blocks.live(), 2u); }

/* Not worth a block: less than `least`, and anything at all with no arena. */
TEST(cl_staging_block, a_small_transfer_or_no_arena_is_not_staged)
{ ClArenaBlocks blocks(lock);
  blocks.lend(arena, sizeof arena);
  { ClStagingBlock staging(blocks);
    CHECK(!staging.take(ClStagingBlock::least - 1u));
    CHECK(!staging.take(0u));
    CHECK(staging.take(ClStagingBlock::least)); }
  ClArenaBlocks none(lock);
  ClStagingBlock staging(none);
  CHECK(!staging.take(1u << 20));
  CHECK_EQ(none.live(), 0u); }
