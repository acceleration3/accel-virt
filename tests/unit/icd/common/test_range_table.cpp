/* Ranges placed first fit in a region, kept sorted, given back by where they
 * begin: what OpenCL's arena blocks, Direct3D 12's heaps and Vulkan's chunk
 * ranges are all built on. Invariants after every change, and every state the
 * table never builds caught by them. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/common/range_table.hpp"

#include <cstdint>

using av::Status;
using av::icd::RangeTable;

namespace av::icd {

/* The rows the table never writes, one per bug the invariant is there for: an
 * overlap is two owners writing each other's bytes, a range past the end is
 * memory nobody has, and a row left behind the count is a range nothing will
 * ever give back. */
struct RangeTableProbe {
    template <std::uint32_t Max>
    static void set_row(RangeTable<Max> &table, std::uint32_t row, std::uint64_t offset, std::uint64_t size)
    {
        table.offsets_[row] = offset;
        table.sizes_[row] = size;
    }
    template <std::uint32_t Max>
    static void set_count(RangeTable<Max> &table, std::uint32_t count) { table.count_ = count; }
    template <std::uint32_t Max>
    static void set_region(RangeTable<Max> &table, std::uint64_t begin, std::uint64_t end)
    {
        table.begin_ = begin;
        table.end_ = end;
    }
};

}  // namespace av::icd

using av::icd::RangeTableProbe;

namespace {

constexpr std::uint64_t page = 4096u;
using Table = RangeTable<4u>;

std::uint64_t placed_at(Table &table, std::uint64_t bytes)
{
    std::uint64_t offset = ~std::uint64_t{0};
    CHECK_STATUS(table.place(bytes, offset), Status::ok);
    CHECK(table.invariants_hold());
    return offset;
}

}  // namespace

TEST(range_table, places_nothing_until_given_a_region)
{ Table table(page);
  std::uint64_t offset = 7u;
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.place(page, offset), Status::no_space);
  CHECK_EQ(offset, 7u);
  CHECK_EQ(table.placed(), 0u);
  CHECK_EQ(table.alignment(), page); }

/* Rounded inward: a region that starts and ends mid-page holds only the whole
 * pages inside it; one that ends before it begins is empty, not wrapped. */
TEST(range_table, the_region_is_rounded_inward_to_the_alignment)
{ Table table(page);
  table.reset(page + 1u, 4u * page + 100u);
  CHECK_EQ(table.begin(), 2u * page);
  CHECK_EQ(table.end(), 4u * page);
  CHECK_EQ(placed_at(table, 1u), 2u * page);
  table.reset(4u * page, page);
  CHECK_EQ(table.begin(), table.end());
  std::uint64_t offset = 0u;
  CHECK_STATUS(table.place(1u, offset), Status::no_space);
  /* A begin that would wrap rounding up leaves nothing to place in. */
  table.reset(~std::uint64_t{0} - 1u, ~std::uint64_t{0});
  CHECK_EQ(table.begin(), table.end());
  CHECK(table.invariants_hold()); }

TEST(range_table, ranges_are_whole_aligned_and_first_fit)
{ Table table(page);
  table.reset(0u, 8u * page);
  CHECK_EQ(placed_at(table, 1u), 0u);
  CHECK_EQ(placed_at(table, page + 1u), page);
  CHECK_EQ(table.size_at(page), 2u * page);
  CHECK_EQ(table.bytes_placed(), 3u * page);
  CHECK_EQ(placed_at(table, page), 3u * page);
  CHECK_EQ(table.placed(), 3u); }

/* A range given back leaves a gap: the next range that fits takes it, and one
 * that does not goes after what is still placed. */
TEST(range_table, a_gap_is_reused_by_the_first_range_that_fits)
{ Table table(page);
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

TEST(range_table, no_bytes_a_size_that_does_not_round_and_a_full_table_are_refused)
{ Table table(page);
  table.reset(0u, 16u * page);
  std::uint64_t offset = 9u;
  CHECK_STATUS(table.place(0u, offset), Status::invalid_parameter);
  CHECK_STATUS(table.place(~std::uint64_t{0}, offset), Status::invalid_parameter);
  CHECK_EQ(offset, 9u);
  for (std::uint32_t row = 0u; row < Table::capacity; row++) static_cast<void>(placed_at(table, page));
  CHECK_STATUS(table.place(page, offset), Status::no_space);
  CHECK_EQ(offset, 9u);
  /* A region too small for the range, with rows to spare. */
  table.reset(0u, page);
  CHECK_STATUS(table.place(2u * page, offset), Status::no_space);
  CHECK(table.invariants_hold()); }

TEST(range_table, only_where_a_range_begins_removes_it)
{ Table table(page);
  table.reset(0u, 4u * page);
  const std::uint64_t at = placed_at(table, 2u * page);
  CHECK_STATUS(table.remove(at + page), Status::invalid_parameter);
  CHECK_EQ(table.size_at(at + page), 0u);
  CHECK_STATUS(table.remove(at), Status::ok);
  CHECK_STATUS(table.remove(at), Status::invalid_parameter);
  CHECK_EQ(table.size_at(at), 0u);
  CHECK_EQ(table.placed(), 0u);
  CHECK(table.invariants_hold()); }

/* A new region forgets every range of the old one, rows and all. */
TEST(range_table, a_new_region_forgets_the_old_ranges)
{ Table table(page);
  table.reset(0u, 4u * page);
  static_cast<void>(placed_at(table, page));
  static_cast<void>(placed_at(table, page));
  table.reset(page, 3u * page);
  CHECK_EQ(table.placed(), 0u);
  CHECK(table.invariants_hold());
  CHECK_EQ(placed_at(table, page), page); }

TEST(range_table, the_invariant_catches_every_state_the_table_never_builds)
{ const auto broken = [](void (*corrupt)(Table &)) {
      Table table(page);
      table.reset(0u, 8u * page);
      static_cast<void>(placed_at(table, page));
      static_cast<void>(placed_at(table, page));
      corrupt(table);
      return table.invariants_hold();
  };
  CHECK(!broken([](Table &t) { RangeTableProbe::set_count(t, Table::capacity + 1u); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 0u, 0u, 0u); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, 0u, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, page + 1u, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, page, page + 1u); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, 7u * page, 2u * page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 1u, 9u * page, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 2u, 2u * page, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_row(t, 2u, 0u, page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_region(t, page + 1u, 8u * page); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_region(t, 0u, 8u * page + 1u); }));
  CHECK(!broken([](Table &t) { RangeTableProbe::set_region(t, 8u * page, 0u); }));
  CHECK(broken([](Table &) {})); }

TEST(range_table, an_alignment_that_is_no_power_of_two_holds_nothing)
{ Table table(3u);
  CHECK(!table.invariants_hold()); }
