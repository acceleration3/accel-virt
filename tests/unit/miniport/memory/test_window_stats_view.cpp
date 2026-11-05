#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "miniport/memory/window_stats_view.hpp"
#include "proto/pages/window_stats.hpp"

#include <cstdint>
#include <cstring>

using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;
using av::WindowStats;
using av::WindowStatsView;

namespace {

const std::uint8_t *page_of(MockPlatform &platform, const WindowStatsView &view)
{
    return static_cast<const std::uint8_t *>(platform.translate_physical(view.address(), WindowStats::page_size));
}

std::uint32_t sequence_of(const std::uint8_t *page)
{
    std::uint32_t sequence = 0u;
    std::memcpy(&sequence, page + WindowStats::sequence_at, sizeof sequence);
    return sequence;
}

}

TEST(window_stats_view, a_view_that_was_never_initialized_has_no_page)
{ MockPlatform   platform;
  WindowStatsView view(platform);
  CHECK(!view.ready());
  CHECK_EQ(view.address(), 0u);
  view.publish(WindowStats(1u, 1u, 1u, 1u, 1u, 1u));
  CHECK(view.invariants_hold()); }

TEST(window_stats_view, initializing_makes_a_zeroed_page_that_already_decodes)
{ MockPlatform   platform;
  WindowStatsView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  CHECK(view.ready());
  CHECK(view.address() != 0u);
  CHECK(view.invariants_hold());
  const std::uint8_t *const page = page_of(platform, view);
  CHECK(page != nullptr);
  WindowStats stats(9u, 9u, 9u, 9u, 9u, 9u);
  CHECK_STATUS(stats.decode(page, WindowStats::encoded_size), Status::ok);
  for (std::uint32_t index = 0u; index < WindowStats::field_count; index++) CHECK_EQ(stats.field(index), 0u);
  for (std::size_t at = WindowStats::encoded_size; at < WindowStats::page_size; at++) CHECK_EQ(page[at], 0u); }

TEST(window_stats_view, initializing_twice_keeps_the_page)
{ MockPlatform   platform;
  WindowStatsView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  const std::uint64_t address = view.address();
  CHECK_STATUS(view.initialize(), Status::ok);
  CHECK_EQ(view.address(), address); }

TEST(window_stats_view, a_refused_allocation_leaves_it_not_ready)
{ MockPlatform platform;
  platform.fail_allocation_at(1u);
  WindowStatsView view(platform);
  CHECK_STATUS(view.initialize(), Status::no_memory);
  CHECK(!view.ready());
  CHECK_EQ(view.address(), 0u);
  CHECK_STATUS(view.initialize(), Status::ok);
  CHECK(view.ready()); }

TEST(window_stats_view, initializing_above_passive_level_is_a_violation_and_allocates_nothing)
{ MockPlatform platform;
  platform.set_irql(Irql::dispatch);
  WindowStatsView view(platform);
  CHECK_STATUS(view.initialize(), Status::no_memory);
  CHECK(!view.ready());
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  platform.set_irql(Irql::passive);
  CHECK_STATUS(view.initialize(), Status::ok); }

TEST(window_stats_view, publishing_writes_the_fields_and_advances_the_sequence_by_two)
{ MockPlatform   platform;
  WindowStatsView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  const std::uint8_t *const page = page_of(platform, view);
  CHECK_EQ(sequence_of(page), 0u);
  view.publish(WindowStats(100u, 40u, 60u, 2u, 0x8000u, 3u));
  CHECK_EQ(sequence_of(page), 2u);
  CHECK(view.invariants_hold());
  WindowStats stats;
  CHECK_STATUS(stats.decode(page, WindowStats::encoded_size), Status::ok);
  CHECK_EQ(stats.total_pages(), 100u);
  CHECK_EQ(stats.pages_in_use(), 40u);
  CHECK_EQ(stats.peak_pages_in_use(), 60u);
  CHECK_EQ(stats.refusals(), 2u);
  CHECK_EQ(stats.refused_bytes(), 0x8000u);
  CHECK_EQ(stats.largest_free_run_at_refusal(), 3u);
  view.publish(WindowStats(100u, 41u, 61u, 2u, 0x8000u, 3u));
  CHECK_EQ(sequence_of(page), 4u);
  CHECK_STATUS(stats.decode(page, WindowStats::encoded_size), Status::ok);
  CHECK_EQ(stats.pages_in_use(), 41u);
  CHECK(view.invariants_hold()); }

TEST(window_stats_view, the_invariant_fails_while_the_sequence_is_odd)
{ MockPlatform   platform;
  WindowStatsView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  const std::uint32_t odd = 1u;
  std::memcpy(const_cast<std::uint8_t *>(page_of(platform, view)) + WindowStats::sequence_at, &odd, sizeof odd);
  CHECK(!view.invariants_hold()); }

TEST(window_stats_view, publishing_above_passive_level_is_a_violation)
{ MockPlatform   platform;
  WindowStatsView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  platform.set_irql(Irql::dispatch);
  view.publish(WindowStats(1u, 1u, 1u, 1u, 1u, 1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
