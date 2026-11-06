#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/retire_list.hpp"

#include <cstdint>
#include <initializer_list>

using av::MockPlatform;
using av::Pool;
using av::Status;
using av::icd::RetireList;

TEST(retire_list, takes_the_newest_entry_first_and_says_when_none_is_left)
{ MockPlatform platform;
  RetireList<std::uint32_t> list(platform, Pool::paged);
  std::uint32_t entry = 0u;
  CHECK(!list.take_last(entry));
  CHECK_STATUS(list.add(1u), Status::ok);
  CHECK_STATUS(list.add(2u), Status::ok);
  CHECK_STATUS(list.add(3u), Status::ok);
  CHECK_EQ(list.count(), 3u);
  CHECK(list.take_last(entry));
  CHECK_EQ(entry, 3u);
  CHECK_EQ(list.count(), 2u);
  CHECK(list.take_last(entry));
  CHECK_EQ(entry, 2u);
  CHECK(list.take_last(entry));
  CHECK_EQ(entry, 1u);
  CHECK(!list.take_last(entry));
  CHECK_EQ(list.count(), 0u); }

TEST(retire_list_take_if, removes_the_first_entry_that_matches_and_leaves_the_rest_in_order)
{ MockPlatform platform;
  RetireList<std::uint32_t> list(platform, Pool::paged);
  for (const std::uint32_t value : {10u, 21u, 30u, 41u}) CHECK_STATUS(list.add(value), Status::ok);
  std::uint32_t entry = 0u;
  CHECK(list.take_if([](std::uint32_t held) { return held % 2u == 1u; }, entry));
  CHECK_EQ(entry, 21u);
  CHECK_EQ(list.count(), 3u);
  CHECK(!list.take_if([](std::uint32_t held) { return held > 100u; }, entry));
  CHECK(list.take_last(entry));
  CHECK_EQ(entry, 41u);
  CHECK(list.take_last(entry));
  CHECK_EQ(entry, 30u);
  CHECK(list.take_last(entry));
  CHECK_EQ(entry, 10u); }

TEST(retire_list_reclaim, gives_every_entry_back_once_and_empties_the_list)
{ MockPlatform platform;
  RetireList<std::uint32_t> list(platform, Pool::paged);
  for (const std::uint32_t value : {4u, 5u, 6u}) CHECK_STATUS(list.add(value), Status::ok);
  std::uint32_t sum = 0u;
  std::uint32_t given = 0u;
  list.reclaim([&sum, &given](std::uint32_t held) {
      sum += held;
      given++;
  });
  CHECK_EQ(sum, 15u);
  CHECK_EQ(given, 3u);
  CHECK_EQ(list.count(), 0u);
  list.reclaim([&given](std::uint32_t) { given++; });
  CHECK_EQ(given, 3u); }
