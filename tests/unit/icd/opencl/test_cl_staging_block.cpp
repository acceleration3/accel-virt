/* The block of a context's arena one transfer stages its bytes in. What a
 * transfer depends on: a block is all of what it says it is and lies inside
 * the arena, it is the whole transfer when the arena has room and as much as
 * the arena can spare when it does not, it is never smaller than worth
 * staging, and it goes back when the transfer is done - whatever happened. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"
#include "icd/opencl/cl_staging_block.hpp"

#include <cstdint>

using av::clicd::ClArenaBlocks;
using av::clicd::ClStagingBlock;

namespace {

alignas(4096) std::uint8_t arena[8u << 20];

av::LockTrace           trace;
av::RecordingThreadLock lock(trace, 'B');
av::MockPlatform        platform;

}  // namespace

TEST(cl_staging_block, a_transfer_the_arena_has_room_for_is_staged_whole_in_pages)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(arena, sizeof arena);
  { ClStagingBlock staging(blocks);
    CHECK(staging.take((3u << 20) + 1u, ClStagingBlock::least));
    CHECK_EQ(staging.bytes(), (3u << 20) + 4096u);
    CHECK(staging.at() == arena);
    CHECK_EQ(blocks.live(), 1u);
    /* One block per transfer: a second take is refused and changes nothing. */
    CHECK(!staging.take(ClStagingBlock::least, ClStagingBlock::least));
    CHECK_EQ(staging.bytes(), (3u << 20) + 4096u);
    CHECK_EQ(blocks.live(), 1u); }
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.invariants_hold()); }

/* More than `most` is `most`: the arena backs buffers too, and one transfer
 * must leave them room. */
TEST(cl_staging_block, a_transfer_larger_than_most_takes_most)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(arena, sizeof arena);
  ClStagingBlock staging(blocks);
  CHECK(staging.take(ClStagingBlock::most + 1u, ClStagingBlock::least));
  CHECK_EQ(staging.bytes(), ClStagingBlock::most);
  ClStagingBlock exactly(blocks);
  CHECK(exactly.take(ClStagingBlock::most, ClStagingBlock::least));
  CHECK_EQ(exactly.bytes(), ClStagingBlock::most);
  CHECK(exactly.at() == arena + ClStagingBlock::most); }

/* A crowded arena lends what it can, halving the ask, and never less than
 * `least`. */
TEST(cl_staging_block, a_crowded_arena_lends_half_and_half_again_down_to_least)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(arena, 1u << 20);
  { ClStagingBlock staging(blocks);
    CHECK(staging.take(3u << 20, ClStagingBlock::least));
    CHECK_EQ(staging.bytes(), 768u << 10); }
  { ClStagingBlock staging(blocks);
    CHECK(staging.take(1536u << 10, ClStagingBlock::least));
    CHECK_EQ(staging.bytes(), 768u << 10); }
  std::uint64_t held = 0u;
  CHECK(blocks.take((1u << 20) - ClStagingBlock::least, held));
  { ClStagingBlock staging(blocks);
    CHECK(staging.take(1u << 20, ClStagingBlock::least));
    CHECK_EQ(staging.bytes(), ClStagingBlock::least); }
  std::uint64_t last = 0u;
  CHECK(blocks.take(4096u, last));
  { ClStagingBlock staging(blocks);
    CHECK(!staging.take(1u << 20, ClStagingBlock::least));
    CHECK(staging.at() == nullptr);
    CHECK_EQ(staging.bytes(), 0u); }
  CHECK_EQ(blocks.live(), 2u); }

/* Not worth a block: less than `least`, and anything at all with no arena. */
TEST(cl_staging_block, a_small_transfer_or_no_arena_is_not_staged)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(arena, sizeof arena);
  { ClStagingBlock staging(blocks);
    CHECK(!staging.take(ClStagingBlock::least - 1u, ClStagingBlock::least));
    CHECK(!staging.take(0u, ClStagingBlock::least));
    CHECK(staging.take(ClStagingBlock::least, ClStagingBlock::least)); }
  ClArenaBlocks none(platform, lock);
  ClStagingBlock staging(none);
  CHECK(!staging.take(1u << 20, ClStagingBlock::least));
  CHECK_EQ(none.live(), 0u); }

/* A caller may find a block of any size worth taking - a read whose rows have
 * gaps comes back inline a row at a time - and then a small transfer is staged
 * whole, in one page, and a crowded arena is halved down to that floor. A
 * transfer of nothing is never staged. */
TEST(cl_staging_block, a_caller_s_own_floor_stages_a_small_transfer)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(arena, sizeof arena);
  { ClStagingBlock staging(blocks);
    CHECK(!staging.take(0u, 1u));
    CHECK(staging.take(1000u, 1u));
    CHECK_EQ(staging.bytes(), 4096u);
    CHECK(staging.at() == arena); }
  { ClStagingBlock staging(blocks);
    CHECK(!staging.take(1000u, 1001u)); }
  ClArenaBlocks crowded(platform, lock);
  crowded.lend(arena, 8192u);
  std::uint64_t held = 0u;
  CHECK(crowded.take(4096u, held));
  ClStagingBlock staging(crowded);
  CHECK(staging.take(12000u, 1u));
  CHECK_EQ(staging.bytes(), 4096u); }
