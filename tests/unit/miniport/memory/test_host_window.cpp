#include "miniport/memory/host_window.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/pages/window_stats.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct HostWindowProbe {
    static void miscount(HostWindow &window, std::uint32_t used) { window.used_ = used; }
    static void set_pages(HostWindow &window, std::uint32_t pages) { window.pages_ = pages; }
    static void set_peak(HostWindow &window, std::uint32_t peak) { window.peak_ = peak; }
    static const WindowStatsView &view(const HostWindow &window) { return window.view_; }
};

}

using av::HostWindow;
using av::HostWindowProbe;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint64_t base = 0x4000000000ull;
constexpr std::uint64_t page = HostWindow::page_bytes;

}

TEST(host_window, takes_the_first_run_that_fits_from_the_start_of_the_window)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK(!window.offered());
  CHECK_STATUS(window.initialize(base, 8u * page), Status::ok);
  CHECK(window.offered());
  std::uint32_t a = 99u, b = 99u, c = 99u;
  CHECK_STATUS(window.take(2u, a), Status::ok);
  CHECK_STATUS(window.take(3u, b), Status::ok);
  CHECK_STATUS(window.take(1u, c), Status::ok);
  CHECK_EQ(a, 0u);
  CHECK_EQ(b, 2u);
  CHECK_EQ(c, 5u);
  CHECK_EQ(window.pages_in_use(), 6u);
  CHECK_EQ(window.physical(b), base + 2u * page);
  window.give_back(a, 2u);
  std::uint32_t small = 99u, large = 99u;
  CHECK_STATUS(window.take(1u, small), Status::ok);
  CHECK_EQ(small, 0u);
  CHECK_STATUS(window.take(2u, large), Status::ok);
  CHECK_EQ(large, 6u);
  CHECK_EQ(window.pages_in_use(), 7u);
  CHECK(window.invariants_hold());
  std::uint32_t none = 99u;
  CHECK_STATUS(window.take(2u, none), Status::no_space);
  CHECK_EQ(none, 0u);
  std::uint32_t last = 99u;
  CHECK_STATUS(window.take(1u, last), Status::ok);
  CHECK_EQ(last, 1u);
  CHECK_STATUS(window.take(1u, none), Status::no_space);
  CHECK_EQ(window.pages_in_use(), 8u);
  CHECK(window.invariants_hold()); }

TEST(host_window, a_chunk_made_and_freed_again_and_again_keeps_landing_on_the_same_pages)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK_STATUS(window.initialize(base, 64u * page), Status::ok);
  for (int round = 0; round < 10; round++) {
      std::uint32_t first = 99u;
      CHECK_STATUS(window.take(4u, first), Status::ok);
      CHECK_EQ(first, 0u);
      window.give_back(first, 4u);
  }
  CHECK_EQ(window.pages_in_use(), 0u); }

TEST(host_window, runs_are_found_across_full_and_partial_words)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK_STATUS(window.initialize(base, 200u * page), Status::ok);
  std::uint32_t full = 99u;
  CHECK_STATUS(window.take(64u, full), Status::ok);
  CHECK_EQ(full, 0u);
  std::uint32_t head = 99u;
  CHECK_STATUS(window.take(60u, head), Status::ok);
  CHECK_EQ(head, 64u);
  std::uint32_t spanning = 99u;
  CHECK_STATUS(window.take(10u, spanning), Status::ok);
  CHECK_EQ(spanning, 124u);
  std::uint32_t tail = 99u;
  CHECK_STATUS(window.take(66u, tail), Status::ok);
  CHECK_EQ(tail, 134u);
  CHECK_EQ(window.pages_in_use(), 200u);
  window.give_back(64u, 60u);
  std::uint32_t reused = 99u;
  CHECK_STATUS(window.take(60u, reused), Status::ok);
  CHECK_EQ(reused, 64u);
  CHECK(window.invariants_hold()); }

TEST(host_window, gives_back_only_what_was_taken_inside_the_window)
{ MockPlatform platform;
  HostWindow window(platform);
  window.give_back(0u, 4u);
  CHECK_EQ(window.pages_in_use(), 0u);
  CHECK_STATUS(window.initialize(base, 8u * page), Status::ok);
  std::uint32_t first = 99u;
  CHECK_STATUS(window.take(2u, first), Status::ok);
  CHECK_STATUS(window.take(2u, first), Status::ok);
  CHECK_EQ(first, 2u);
  window.give_back(1u, 2u);
  CHECK_EQ(window.pages_in_use(), 2u);
  window.give_back(0u, 8u);
  CHECK_EQ(window.pages_in_use(), 0u);
  CHECK_STATUS(window.take(8u, first), Status::ok);
  window.give_back(8u, 1u);
  window.give_back(6u, UINT32_MAX);
  CHECK_EQ(window.pages_in_use(), 6u);
  CHECK(window.invariants_hold()); }

TEST(host_window, refuses_what_it_cannot_take)
{ MockPlatform platform;
  HostWindow window(platform);
  std::uint32_t first = 99u;
  CHECK_STATUS(window.take(1u, first), Status::not_supported);
  CHECK_EQ(first, 0u);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  CHECK_STATUS(window.take(0u, first), Status::invalid_parameter);
  CHECK_STATUS(window.take(5u, first), Status::no_space);
  CHECK_EQ(window.pages_in_use(), 0u); }

TEST(host_window, a_withdrawn_window_takes_nothing_and_still_takes_back)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  std::uint32_t first = 99u;
  CHECK_STATUS(window.take(2u, first), Status::ok);
  window.withdraw();
  CHECK(!window.offered());
  std::uint32_t refused = 99u;
  CHECK_STATUS(window.take(1u, refused), Status::not_supported);
  window.give_back(first, 2u);
  CHECK_EQ(window.pages_in_use(), 0u);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  CHECK(window.offered());
  CHECK_STATUS(window.take(4u, first), Status::ok); }

TEST(host_window_initialize, keeps_its_map_and_judges_a_region_found_again)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK_STATUS(window.initialize(base, 0u), Status::invalid_parameter);
  CHECK_STATUS(window.initialize(base, page - 1u), Status::invalid_parameter);
  CHECK_STATUS(window.initialize(base, (std::uint64_t{UINT32_MAX} + 1u) * page), Status::invalid_parameter);
  CHECK(!window.offered());
  CHECK_STATUS(window.initialize(base, 4u * page + 100u), Status::ok);
  const std::uint32_t made = platform.allocations_made();
  std::uint32_t first = 99u;
  CHECK_STATUS(window.take(4u, first), Status::ok);
  CHECK_STATUS(window.take(1u, first), Status::no_space);
  window.withdraw();
  CHECK_STATUS(window.initialize(base, 8u * page), Status::invalid_state);
  CHECK_STATUS(window.initialize(base + page, 4u * page), Status::invalid_state);
  CHECK(!window.offered());
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  window.give_back(0u, 4u);
  window.withdraw();
  CHECK_STATUS(window.initialize(base + page, 4u * page), Status::ok);
  CHECK_EQ(window.physical(0u), base + page);
  CHECK_EQ(platform.allocations_made(), made); }

TEST(host_window_initialize, a_map_that_cannot_be_allocated_offers_nothing)
{ MockPlatform platform;
  HostWindow window(platform);
  platform.fail_allocation_at(2u);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::no_memory);
  CHECK(!window.offered());
  std::uint32_t first = 99u;
  CHECK_STATUS(window.take(1u, first), Status::not_supported);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  CHECK(window.invariants_hold()); }

TEST(host_window_initialize, a_stats_page_that_cannot_be_allocated_is_logged_and_the_window_is_offered_without_it)
{ MockPlatform platform;
  HostWindow window(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  CHECK(window.offered());
  CHECK(platform.logged() >= 1u);
  CHECK(std::strstr(platform.last_log(), "the host window stats page is unavailable") != nullptr);
  std::uint32_t first = 99u;
  CHECK_STATUS(window.take(1u, first), Status::ok);
  CHECK(window.invariants_hold()); }

TEST(host_window, every_call_is_passive_only)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  platform.set_irql(Irql::dispatch);
  std::uint32_t first = 0u;
  static_cast<void>(window.initialize(base, 4u * page));
  static_cast<void>(window.take(1u, first));
  window.give_back(first, 1u);
  window.withdraw();
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {}
  while (platform.claim(Violation::blocking_at_dispatch)) {} }

TEST(host_window_invariants, a_count_that_does_not_match_the_map_is_caught)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK(window.invariants_hold());
  HostWindowProbe::miscount(window, 1u);
  CHECK(!window.invariants_hold());
  HostWindowProbe::miscount(window, 0u);
  HostWindowProbe::set_pages(window, 3u);
  CHECK(!window.invariants_hold());
  HostWindowProbe::set_pages(window, 0u);
  CHECK_STATUS(window.initialize(base, 4u * page), Status::ok);
  std::uint32_t first = 0u;
  CHECK_STATUS(window.take(2u, first), Status::ok);
  CHECK(window.invariants_hold());
  HostWindowProbe::miscount(window, 3u);
  CHECK(!window.invariants_hold());
  HostWindowProbe::miscount(window, 2u);
  HostWindowProbe::set_pages(window, 1u);
  CHECK(!window.invariants_hold());
  HostWindowProbe::set_pages(window, 4u);
  CHECK(window.invariants_hold());
  HostWindowProbe::set_peak(window, 1u);
  CHECK(!window.invariants_hold());
  HostWindowProbe::set_peak(window, 5u);
  CHECK(!window.invariants_hold());
  HostWindowProbe::set_peak(window, 2u);
  CHECK(window.invariants_hold()); }

TEST(host_window, a_refusal_says_the_largest_run_left_past_the_words_that_are_full)
{ MockPlatform platform;
  HostWindow window(platform);
  CHECK_STATUS(window.initialize(base, 200u * page), Status::ok);
  std::uint32_t first = 99u;
  CHECK_STATUS(window.take(64u, first), Status::ok);
  CHECK_STATUS(window.take(100u, first), Status::ok);
  CHECK_EQ(first, 64u);
  CHECK_STATUS(window.take(37u, first), Status::no_space);
  const av::WindowStatsView &view = HostWindowProbe::view(window);
  const auto *const bytes =
      static_cast<const std::uint8_t *>(platform.translate_physical(view.address(), av::WindowStats::page_size));
  av::WindowStats stats;
  CHECK_STATUS(stats.decode(bytes, av::WindowStats::encoded_size), Status::ok);
  CHECK_EQ(stats.refusals(), 1u);
  CHECK_EQ(stats.refused_bytes(), 37u * page);
  CHECK_EQ(stats.largest_free_run_at_refusal(), 36u);
  window.give_back(0u, 64u);
  CHECK_STATUS(window.take(60u, first), Status::ok);
  CHECK_STATUS(window.take(37u, first), Status::no_space);
  CHECK_STATUS(stats.decode(bytes, av::WindowStats::encoded_size), Status::ok);
  CHECK_EQ(stats.largest_free_run_at_refusal(), 36u);
  CHECK(window.invariants_hold()); }
