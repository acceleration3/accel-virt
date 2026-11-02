#include "core/platform/block_source.hpp"
#include "device/host/process_block_source.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>

using av::Irql;
using av::Pool;
using av::ProcessBlockSource;

TEST(process_block_source, gives_zeroed_blocks_and_counts_the_ones_not_taken_back)
{ ProcessBlockSource blocks;
  CHECK_EQ(blocks.live(), 0u);
  auto *const first = static_cast<std::uint8_t *>(blocks.allocate(64u, Pool::paged));
  void *const second = blocks.allocate(16u, Pool::non_paged);
  CHECK(first != nullptr && second != nullptr);
  for (std::size_t at = 0u; at < 64u; at++) CHECK_EQ(first[at], 0u);
  CHECK_EQ(blocks.live(), 2u);
  blocks.require_irql_at_most(Irql::apc, "test");
  blocks.release(first);
  CHECK_EQ(blocks.live(), 1u);
  blocks.release(second);
  CHECK_EQ(blocks.live(), 0u); }
