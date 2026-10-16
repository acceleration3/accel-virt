/* WatchedBlocks: every request passed to the source under it, and a command
 * that failed because one was refused told apart from one that failed for
 * any other reason - the guest learns E_OUTOFMEMORY for the first. */
#include "core/support/growable_array.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::GrowableArray;
using av::Irql;
using av::MockPlatform;
using av::Pool;
using av::Status;
using av::Violation;
using av::WatchedBlocks;

namespace {

constexpr std::uint32_t internal = static_cast<std::uint32_t>(DXGI_ERROR_DRIVER_INTERNAL_ERROR);

}  // namespace

TEST(watched_blocks_allocate, passes_requests_and_releases_to_the_source_under_it)
{ MockPlatform  platform;
  WatchedBlocks blocks(platform);
  void *const   block = blocks.allocate(64u, Pool::paged);
  CHECK(block != nullptr);
  CHECK_EQ(platform.live_allocations(), 1u);
  CHECK_EQ(blocks.refusal(internal), internal);
  blocks.release(block);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(watched_blocks_refusal, a_refused_request_makes_the_failure_out_of_memory_until_the_next_command)
{ MockPlatform  platform;
  WatchedBlocks blocks(platform);
  GrowableArray<std::uint32_t> grown(blocks, Pool::paged);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(grown.append(1u), Status::no_memory);
  CHECK_EQ(blocks.refusal(internal), static_cast<std::uint32_t>(E_OUTOFMEMORY));
  CHECK_EQ(blocks.refusal(0u), static_cast<std::uint32_t>(E_OUTOFMEMORY));
  blocks.forget();
  CHECK_EQ(blocks.refusal(internal), internal);
  CHECK_STATUS(grown.append(1u), Status::ok);
  CHECK_EQ(blocks.refusal(internal), internal); }

TEST(watched_blocks_irql, the_ceiling_is_checked_by_the_source_under_it)
{ MockPlatform  platform;
  WatchedBlocks blocks(platform);
  platform.set_irql(Irql::dispatch);
  blocks.require_irql_at_most(Irql::apc, "test");
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
