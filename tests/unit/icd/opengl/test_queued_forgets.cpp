#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/queued_forgets.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::glicd::QueuedForgets;

TEST(queued_forgets, starts_empty)
{ MockPlatform  platform;
  QueuedForgets forgets(platform);
  std::uint32_t buffer = 77u;
  CHECK_EQ(forgets.waiting(), 0u);
  CHECK(!forgets.take(buffer));
  CHECK_EQ(buffer, 77u);
  CHECK(forgets.invariants_hold()); }

TEST(queued_forgets, hands_back_the_newest_name_first_and_counts_down)
{ MockPlatform  platform;
  QueuedForgets forgets(platform);
  CHECK_STATUS(forgets.add(10u), Status::ok);
  CHECK_STATUS(forgets.add(20u), Status::ok);
  CHECK_STATUS(forgets.add(30u), Status::ok);
  CHECK_EQ(forgets.waiting(), 3u);
  CHECK(forgets.invariants_hold());
  std::uint32_t buffer = 0u;
  CHECK(forgets.take(buffer));
  CHECK_EQ(buffer, 30u);
  CHECK_EQ(forgets.waiting(), 2u);
  CHECK(forgets.take(buffer));
  CHECK_EQ(buffer, 20u);
  CHECK(forgets.take(buffer));
  CHECK_EQ(buffer, 10u);
  CHECK(!forgets.take(buffer));
  CHECK_EQ(forgets.waiting(), 0u);
  CHECK(forgets.invariants_hold()); }

TEST(queued_forgets, the_same_name_queued_twice_is_forgotten_twice)
{ MockPlatform  platform;
  QueuedForgets forgets(platform);
  CHECK_STATUS(forgets.add(5u), Status::ok);
  CHECK_STATUS(forgets.add(5u), Status::ok);
  std::uint32_t buffer = 0u;
  CHECK(forgets.take(buffer));
  CHECK(forgets.take(buffer));
  CHECK(!forgets.take(buffer)); }

TEST(queued_forgets, any_name_including_zero_and_the_largest_is_kept_verbatim)
{ MockPlatform  platform;
  QueuedForgets forgets(platform);
  CHECK_STATUS(forgets.add(0u), Status::ok);
  CHECK_STATUS(forgets.add(0xFFFFFFFFu), Status::ok);
  std::uint32_t buffer = 1u;
  CHECK(forgets.take(buffer));
  CHECK_EQ(buffer, 0xFFFFFFFFu);
  CHECK(forgets.take(buffer));
  CHECK_EQ(buffer, 0u); }

TEST(queued_forgets, a_failed_allocation_queues_nothing_and_the_next_add_succeeds)
{ MockPlatform  platform;
  QueuedForgets forgets(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(forgets.add(1u), Status::no_memory);
  CHECK_EQ(forgets.waiting(), 0u);
  std::uint32_t buffer = 0u;
  CHECK(!forgets.take(buffer));
  CHECK(forgets.invariants_hold());
  CHECK_STATUS(forgets.add(2u), Status::ok);
  CHECK_EQ(forgets.waiting(), 1u);
  CHECK(forgets.take(buffer));
  CHECK_EQ(buffer, 2u); }

TEST(queued_forgets, a_long_queue_drains_completely_in_order)
{ MockPlatform  platform;
  QueuedForgets forgets(platform);
  for (std::uint32_t name = 1u; name <= 5000u; name++) CHECK_STATUS(forgets.add(name), Status::ok);
  CHECK_EQ(forgets.waiting(), 5000u);
  CHECK(forgets.invariants_hold());
  std::uint32_t buffer = 0u;
  for (std::uint32_t expected = 5000u; expected >= 1u; expected--) {
      CHECK(forgets.take(buffer));
      CHECK_EQ(buffer, expected);
  }
  CHECK_EQ(forgets.waiting(), 0u);
  CHECK(forgets.invariants_hold()); }

TEST(queued_forgets, clear_forgets_everything_and_the_queue_is_usable_again)
{ MockPlatform  platform;
  QueuedForgets forgets(platform);
  CHECK_STATUS(forgets.add(1u), Status::ok);
  CHECK_STATUS(forgets.add(2u), Status::ok);
  forgets.clear();
  CHECK_EQ(forgets.waiting(), 0u);
  std::uint32_t buffer = 0u;
  CHECK(!forgets.take(buffer));
  CHECK(forgets.invariants_hold());
  CHECK_STATUS(forgets.add(9u), Status::ok);
  CHECK(forgets.take(buffer));
  CHECK_EQ(buffer, 9u);
  forgets.clear();
  forgets.clear();
  CHECK_EQ(forgets.waiting(), 0u); }
