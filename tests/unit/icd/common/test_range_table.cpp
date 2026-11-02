#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/range_table.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::icd::RangeTable;

namespace av::icd {
struct RangeTableProbe {
    static void set_row(RangeTable &table, std::uint32_t row, std::uint64_t offset, std::uint64_t size)
    {
        table.ranges_.at(row) = RangeTable::Range{offset, size};
    }
    static void set_region(RangeTable &table, std::uint64_t begin, std::uint64_t end)
    {
        table.begin_ = begin;
        table.end_ = end;
    }
};

}

using av::icd::RangeTableProbe;

namespace {

constexpr std::uint64_t page = 4096u;
using Table = RangeTable;

std::uint64_t placed_at(Table &table, std::uint64_t bytes)
{
    std::uint64_t offset = ~std::uint64_t{0};
    CHECK_STATUS(table.place(bytes, offset), Status::ok);
    CHECK(table.invariants_hold());
    return offset;
}

}

TEST(range_table, places_nothing_until_given_a_region)
{ MockPlatform platform;
  Table table(platform, page);
  std::uint64_t offset = 7u;
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.place(page, offset), Status::no_space);
  CHECK_EQ(offset, 7u);
  CHECK_EQ(table.placed(), 0u);
  CHECK_EQ(table.alignment(), page); }

TEST(range_table, the_region_is_rounded_inward_to_the_alignment)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(page + 1u, 4u * page + 100u);
  CHECK_EQ(table.begin(), 2u * page);
  CHECK_EQ(table.end(), 4u * page);
  CHECK_EQ(placed_at(table, 1u), 2u * page);
  table.reset(4u * page, page);
  CHECK_EQ(table.begin(), table.end());
  std::uint64_t offset = 0u;
  CHECK_STATUS(table.place(1u, offset), Status::no_space);
  table.reset(~std::uint64_t{0} - 1u, ~std::uint64_t{0});
  CHECK_EQ(table.begin(), table.end());
  CHECK(table.invariants_hold()); }

TEST(range_table, ranges_are_whole_aligned_and_first_fit)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(0u, 8u * page);
  CHECK_EQ(placed_at(table, 1u), 0u);
  CHECK_EQ(placed_at(table, page + 1u), page);
  CHECK_EQ(table.size_at(page), 2u * page);
  CHECK_EQ(table.bytes_placed(), 3u * page);
  CHECK_EQ(placed_at(table, page), 3u * page);
  CHECK_EQ(table.placed(), 3u); }

TEST(range_table, a_gap_is_reused_by_the_first_range_that_fits)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(0u, 8u * page);
  const std::uint64_t a = placed_at(table, 2u * page);
  const std::uint64_t b = placed_at(table, 2u * page);
  const std::uint64_t c = placed_at(table, 2u * page);
  CHECK_STATUS(table.remove(b), Status::ok);
  CHECK(table.invariants_hold());
  std::uint64_t offset = 0u;
  CHECK_STATUS(table.place(3u * page, offset), Status::no_space);
  CHECK_EQ(placed_at(table, page), b);
  CHECK_EQ(placed_at(table, page), b + page);
  CHECK_EQ(table.size_at(a), 2u * page);
  CHECK_EQ(table.size_at(c), 2u * page);
  CHECK_EQ(table.placed(), 4u); }

TEST(range_table, no_bytes_a_size_that_does_not_round_and_a_full_region_are_refused)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(0u, 16u * page);
  std::uint64_t offset = 9u;
  CHECK_STATUS(table.place(0u, offset), Status::invalid_parameter);
  CHECK_STATUS(table.place(~std::uint64_t{0}, offset), Status::invalid_parameter);
  CHECK_EQ(offset, 9u);
  for (std::uint32_t row = 0u; row < 16u; row++) static_cast<void>(placed_at(table, page));
  CHECK_STATUS(table.place(page, offset), Status::no_space);
  CHECK_EQ(offset, 9u);
  table.reset(0u, page);
  CHECK_STATUS(table.place(2u * page, offset), Status::no_space);
  CHECK(table.invariants_hold()); }

TEST(range_table, places_as_many_ranges_as_the_region_holds)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(0u, 5000u * page);
  for (std::uint32_t row = 0u; row < 5000u; row++) {
      std::uint64_t offset = 0u;
      CHECK_STATUS(table.place(page, offset), Status::ok);
      CHECK_EQ(offset, row * page);
  }
  CHECK(table.invariants_hold());
  std::uint64_t offset = 0u;
  CHECK_STATUS(table.place(page, offset), Status::no_space);
  for (std::uint32_t row = 0u; row < 5000u; row += 2u) CHECK_STATUS(table.remove(row * page), Status::ok);
  CHECK_EQ(table.placed(), 2500u);
  CHECK_EQ(table.size_at(4999u * page), page);
  CHECK_EQ(table.size_at(4998u * page), 0u);
  CHECK(table.invariants_hold()); }

TEST(range_table, a_range_with_no_memory_for_its_row_is_refused_and_nothing_changes)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(0u, 64u * page);
  for (std::uint32_t row = 0u; row < 8u; row++) static_cast<void>(placed_at(table, page));
  platform.fail_allocation_at(1u);
  std::uint64_t offset = 5u;
  CHECK_STATUS(table.place(page, offset), Status::no_memory);
  CHECK_EQ(offset, 5u);
  CHECK_EQ(table.placed(), 8u);
  CHECK(table.invariants_hold());
  CHECK_EQ(placed_at(table, page), 8u * page); }

TEST(range_table, only_where_a_range_begins_removes_it)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(0u, 4u * page);
  const std::uint64_t at = placed_at(table, 2u * page);
  CHECK_STATUS(table.remove(at + page), Status::invalid_parameter);
  CHECK_EQ(table.size_at(at + page), 0u);
  CHECK_STATUS(table.remove(at), Status::ok);
  CHECK_STATUS(table.remove(at), Status::invalid_parameter);
  CHECK_EQ(table.size_at(at), 0u);
  CHECK_EQ(table.placed(), 0u);
  CHECK(table.invariants_hold()); }

TEST(range_table, a_new_region_forgets_the_old_ranges)
{ MockPlatform platform;
  Table table(platform, page);
  table.reset(0u, 4u * page);
  static_cast<void>(placed_at(table, page));
  static_cast<void>(placed_at(table, page));
  table.reset(page, 3u * page);
  CHECK_EQ(table.placed(), 0u);
  CHECK(table.invariants_hold());
  CHECK_EQ(placed_at(table, page), page); }

TEST(range_table, the_invariant_catches_every_state_the_table_never_builds)
{ const auto broken = [](void (*corrupt)(Table &)) {
      MockPlatform platform;
      Table        table(platform, page);
      table.reset(0u, 8u * page);
      static_cast<void>(placed_at(table, page));
      static_cast<void>(placed_at(table, page));
      corrupt(table);
      return table.invariants_hold();
  };
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 0u, 0u, 0u); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, 0u, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, page + 1u, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, page, page + 1u); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, 7u * page, 2u * page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, 9u * page, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_region(t, page + 1u, 8u * page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_region(t, 0u, 8u * page + 1u); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_region(t, 8u * page, 0u); }));
  CHECK(broken([](Table &) {})); }

TEST(range_table, an_alignment_that_is_no_power_of_two_holds_nothing)
{ MockPlatform platform;
  Table        table(platform, 3u);
  CHECK(!table.invariants_hold()); }
