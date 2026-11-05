#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"

#include <cstdint>

using av::clicd::ClArenaBlocks;

namespace av::icd {
struct RangeTableProbe {
    static void overlap(RangeTable &table) { table.ranges_.at(1u) = table.ranges_.at(0u); }
};

}
using av::clicd::ClArenaBlocksProbe;

namespace av::clicd {
struct ClArenaBlocksProbe {
    static void blocks_past_the_arena(ClArenaBlocks &blocks) { blocks.bytes_ = 4096u; }
    static void capacity_without_an_arena(ClArenaBlocks &blocks)
    { blocks.base_ = nullptr; blocks.bytes_ = 4096u; }
    static void blocks_without_an_arena(ClArenaBlocks &blocks)
    { blocks.base_ = nullptr; blocks.bytes_ = 0u; }
    static void rows_past_the_table(ClArenaBlocks &blocks) { icd::RangeTableProbe::overlap(blocks.blocks_); }
};

}

namespace {

alignas(4096) std::uint8_t arena[64u * 4096u];
constexpr std::uint32_t    many = 2048u;
alignas(4096) std::uint8_t roomy[many * 4096u];

av::LockTrace           trace;
av::RecordingThreadLock lock(trace, 'B');
av::MockPlatform        platform;

}

TEST(cl_arena_blocks, lends_nothing_until_it_is_given_an_arena)
{ ClArenaBlocks blocks(platform, lock);
  std::uint64_t offset = 0xEEu;
  CHECK(!blocks.ready());
  CHECK(!blocks.take(4096u, offset));
  CHECK_EQ(offset, 0xEEu);
  CHECK(blocks.at(0u, 1u) == nullptr);
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, a_block_is_the_arena_s_own_memory_at_the_offset_it_answered)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, arena, sizeof arena);
  CHECK(blocks.ready());
  std::uint64_t first = 0xEEu;
  CHECK(blocks.take(128u, first));
  CHECK_EQ(first, 0u);
  CHECK(blocks.at(first, 128u) == arena);
  CHECK_EQ(blocks.live(), 1u);

  std::uint64_t second = 0u;
  CHECK(blocks.take(128u, second));
  CHECK_EQ(second, 4096u);
  CHECK(blocks.at(second, 128u) == arena + 4096u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, reading_past_a_block_or_outside_every_block_answers_nothing)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, arena, sizeof arena);
  std::uint64_t taken = 0u;
  CHECK(blocks.take(128u, taken));
  CHECK(blocks.at(taken, 128u) == arena);
  CHECK(blocks.at(taken, 4097u) == nullptr);
  CHECK(blocks.at(8192u, 1u) == nullptr);
  CHECK(blocks.at(~std::uint64_t{0}, 1u) == nullptr);
  CHECK(blocks.at(taken, ~std::uint64_t{0}) == nullptr);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, a_block_given_back_is_handed_out_again)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, arena, sizeof arena);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK(blocks.take(128u, first));
  CHECK(blocks.take(128u, second));
  CHECK_EQ(blocks.live(), 2u);

  blocks.give_back(first);
  CHECK_EQ(blocks.live(), 1u);
  CHECK(blocks.at(first, 128u) == nullptr);
  CHECK(blocks.invariants_hold());

  std::uint64_t again = 0xEEu;
  CHECK(blocks.take(128u, again));
  CHECK_EQ(again, first);
  CHECK(blocks.at(again, 128u) == arena);
  CHECK_EQ(blocks.live(), 2u);
  CHECK(blocks.at(second, 128u) == arena + 4096u);
  CHECK(blocks.invariants_hold());

  blocks.give_back(0x9000u);
  blocks.give_back(0xEEEEu);
  CHECK_EQ(blocks.live(), 2u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, an_arena_with_no_room_refuses_rather_than_overlapping_what_is_live)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, arena, 2u * 4096u);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  std::uint64_t third = 0xEEu;
  CHECK(blocks.take(4096u, first));
  CHECK(blocks.take(4096u, second));
  CHECK(!blocks.take(1u, third));
  CHECK_EQ(third, 0xEEu);
  CHECK_EQ(blocks.live(), 2u);
  CHECK(blocks.invariants_hold());

  ClArenaBlocks empty(platform, lock);
  empty.lend(0u, arena, 2u * 4096u);
  std::uint64_t huge = 0xEEu;
  CHECK(!empty.take(3u * 4096u, huge));
  CHECK(!empty.take(~std::uint64_t{0}, huge));
  CHECK_EQ(huge, 0xEEu);
  CHECK_EQ(empty.live(), 0u);
  CHECK(empty.invariants_hold()); }

TEST(cl_arena_blocks, a_block_of_nothing_is_not_a_block)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, arena, sizeof arena);
  std::uint64_t offset = 0xEEu;
  CHECK(!blocks.take(0u, offset));
  CHECK_EQ(offset, 0xEEu);
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, backs_a_block_on_every_page_of_the_arena)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, roomy, sizeof roomy);
  std::uint64_t offset = 0u;
  for (std::uint32_t index = 0u; index < many; ++index) {
      CHECK(blocks.take(16u, offset));
      CHECK_EQ(offset, std::uint64_t{index} * 4096u);
  }
  CHECK_EQ(blocks.live(), many);
  std::uint64_t one_more = 0xEEu;
  CHECK(!blocks.take(16u, one_more));
  CHECK_EQ(one_more, 0xEEu);
  CHECK(blocks.at(std::uint64_t{many - 1u} * 4096u, 16u) == roomy + std::uint64_t{many - 1u} * 4096u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, no_memory_for_a_row_refuses_and_keeps_every_block)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, roomy, sizeof roomy);
  std::uint64_t offset = 0u;
  for (std::uint32_t index = 0u; index < 8u; ++index) CHECK(blocks.take(16u, offset));
  platform.fail_allocation_at(1u);
  std::uint64_t refused = 0xEEu;
  CHECK(!blocks.take(16u, refused));
  CHECK_EQ(refused, 0xEEu);
  CHECK_EQ(blocks.live(), 8u);
  CHECK(blocks.take(16u, offset));
  CHECK_EQ(offset, 8u * 4096u);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, a_new_arena_forgets_every_block_of_the_old_one)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, arena, sizeof arena);
  std::uint64_t taken = 0u;
  CHECK(blocks.take(128u, taken));
  CHECK_EQ(blocks.live(), 1u);
  blocks.lend(0u, nullptr, 0u);
  CHECK(!blocks.ready());
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.at(taken, 128u) == nullptr);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, the_invariant_catches_the_states_it_is_there_to_catch)
{ const auto broken = [](void (*corrupt)(ClArenaBlocks &)) {
      ClArenaBlocks blocks(platform, lock);
      blocks.lend(0u, arena, sizeof arena);
      std::uint64_t first = 0u;
      std::uint64_t second = 0u;
      CHECK(blocks.take(128u, first));
      CHECK(blocks.take(128u, second));
      CHECK(blocks.invariants_hold());
      corrupt(blocks);
      return blocks.invariants_hold();
  };
  CHECK(!broken(ClArenaBlocksProbe::blocks_past_the_arena));
  CHECK(!broken(ClArenaBlocksProbe::capacity_without_an_arena));
  CHECK(!broken(ClArenaBlocksProbe::blocks_without_an_arena));
  CHECK(!broken(ClArenaBlocksProbe::rows_past_the_table)); }

TEST(cl_arena_blocks, a_read_of_no_bytes_names_nothing)
{ ClArenaBlocks blocks(platform, lock);
  blocks.lend(0u, arena, sizeof arena);
  std::uint64_t taken = 0u;
  CHECK(blocks.take(128u, taken));
  CHECK(blocks.at(taken, 0u) == nullptr);
  CHECK(blocks.invariants_hold()); }

TEST(cl_arena_blocks, every_call_holds_the_lock_and_lets_it_go)
{ ClArenaBlocks       blocks(platform, lock);
  const std::uint32_t before = lock.acquisitions();
  blocks.lend(0u, arena, sizeof arena);
  std::uint64_t offset = 0u;
  CHECK(blocks.take(4096u, offset));
  CHECK(blocks.ready());
  CHECK(blocks.at(offset, 1u) == arena);
  CHECK_EQ(blocks.live(), 1u);
  blocks.give_back(offset);
  CHECK(blocks.invariants_hold());
  CHECK_EQ(lock.acquisitions(), before + 7u);
  CHECK(!lock.held()); }

TEST(cl_arena_blocks, blocks_lent_from_a_region_are_named_by_arena_offsets_and_resolve_back)
{ const std::uint64_t base = std::uint64_t{3} << 26u;
  ClArenaBlocks blocks(platform, lock);
  blocks.lend(base, arena, sizeof arena);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK(blocks.take(4096u, first));
  CHECK(blocks.take(128u, second));
  CHECK_EQ(first, base);
  CHECK_EQ(second, base + 4096u);
  CHECK(blocks.at(first, 4096u) == arena);
  CHECK(blocks.at(second, 128u) == arena + 4096u);
  CHECK(blocks.at(second - 4096u, 1u) == arena);
  CHECK(blocks.at(base - 1u, 1u) == nullptr);
  CHECK(blocks.at(0u, 1u) == nullptr);
  CHECK(blocks.at(second, 4097u) == nullptr);
  blocks.give_back(first);
  CHECK_EQ(blocks.live(), 1u);
  blocks.retire(second);
  blocks.reclaim();
  CHECK_EQ(blocks.live(), 0u);
  CHECK(blocks.invariants_hold());
  blocks.lend(0u, nullptr, 0u);
  CHECK(!blocks.ready());
  CHECK(blocks.invariants_hold()); }
