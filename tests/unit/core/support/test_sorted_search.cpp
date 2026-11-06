#include "core/support/growable_array.hpp"
#include "core/support/sorted_search.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <initializer_list>

namespace av {

struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_size(GrowableArray<T> &array, std::uint32_t size) { array.size_ = size; }
};

}

using av::GrowableArray;
using av::GrowableArrayProbe;
using av::MockPlatform;
using av::Pool;
using av::SortedSearch;
using av::Status;

namespace {

struct Range {
    std::uint64_t begin;
    std::uint64_t size;
};

std::uint64_t end_of(const Range &range) { return range.begin + range.size; }

}

TEST(sorted_search, finds_the_row_at_or_after_and_above_a_key_in_plain_keys)
{ MockPlatform platform;
  GrowableArray<std::uint64_t> rows(platform, Pool::paged);
  for (const std::uint64_t key : {10u, 20u, 20u, 30u}) CHECK_STATUS(rows.append(key), Status::ok);
  const SortedSearch<std::uint64_t> search(rows);
  CHECK(search.invariants_hold());
  CHECK_EQ(search.at_or_after(5u), 0u);
  CHECK_EQ(search.at_or_after(10u), 0u);
  CHECK_EQ(search.at_or_after(11u), 1u);
  CHECK_EQ(search.at_or_after(20u), 1u);
  CHECK_EQ(search.at_or_after(31u), 4u);
  CHECK_EQ(search.above(20u), 3u);
  CHECK_EQ(search.above(30u), 4u);
  CHECK_EQ(search.above(~std::uint64_t{0}), 4u); }

TEST(sorted_search, finds_the_row_at_or_below_a_key_and_the_exact_one)
{ MockPlatform platform;
  GrowableArray<std::uint64_t> rows(platform, Pool::paged);
  for (const std::uint64_t key : {10u, 20u, 30u}) CHECK_STATUS(rows.append(key), Status::ok);
  const SortedSearch<std::uint64_t> search(rows);
  CHECK_EQ(search.at_or_below(9u), 3u);
  CHECK_EQ(search.at_or_below(10u), 0u);
  CHECK_EQ(search.at_or_below(25u), 1u);
  CHECK_EQ(search.at_or_below(30u), 2u);
  CHECK_EQ(search.at_or_below(1000u), 2u);
  CHECK_EQ(search.exact(20u), 1u);
  CHECK_EQ(search.exact(21u), 3u);
  CHECK_EQ(search.exact(5u), 3u); }

TEST(sorted_search, reads_its_key_from_a_member_or_a_function_of_the_row)
{ MockPlatform platform;
  GrowableArray<Range> rows(platform, Pool::paged);
  CHECK_STATUS(rows.append(Range{0u, 16u}), Status::ok);
  CHECK_STATUS(rows.append(Range{16u, 16u}), Status::ok);
  CHECK_STATUS(rows.append(Range{64u, 8u}), Status::ok);
  const SortedSearch<Range, &Range::begin> by_begin(rows);
  const SortedSearch<Range, end_of> by_end(rows);
  CHECK_EQ(by_begin.at_or_below(20u), 1u);
  CHECK_EQ(by_begin.exact(64u), 2u);
  CHECK_EQ(by_end.at_or_after(17u), 1u);
  CHECK_EQ(by_end.exact(72u), 2u);
  CHECK(by_begin.invariants_hold());
  CHECK(by_end.invariants_hold()); }

TEST(sorted_search, an_empty_array_holds_nothing_and_unsorted_rows_break_the_invariant)
{ MockPlatform platform;
  GrowableArray<std::uint64_t> rows(platform, Pool::paged);
  const SortedSearch<std::uint64_t> search(rows);
  CHECK_EQ(search.at_or_after(1u), 0u);
  CHECK_EQ(search.at_or_below(1u), 0u);
  CHECK_EQ(search.exact(1u), 0u);
  CHECK(search.invariants_hold());
  CHECK_STATUS(rows.append(30u), Status::ok);
  CHECK_STATUS(rows.append(10u), Status::ok);
  CHECK(!search.invariants_hold()); }

TEST(sorted_search, rows_that_break_their_own_invariant_break_the_search_s)
{ MockPlatform platform;
  GrowableArray<std::uint64_t> rows(platform, Pool::paged);
  CHECK_STATUS(rows.append(10u), Status::ok);
  const SortedSearch<std::uint64_t> search(rows);
  CHECK(search.invariants_hold());
  GrowableArrayProbe::corrupt_size(rows, 9u);
  CHECK(!search.invariants_hold());
  GrowableArrayProbe::corrupt_size(rows, 1u);
  CHECK(search.invariants_hold()); }
