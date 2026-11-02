#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/svm_table.hpp"

#include <cstdint>
#include <cstring>

using av::MockPlatform;
using av::StateWriter;
using av::clicd::SvmAllocation;
using av::clicd::SvmTable;
using av::clicd::SvmAllocationProbe;
using av::clicd::SvmTableProbe;

namespace av::clicd {
struct SvmAllocationProbe {
    static void forget_host_base(SvmAllocation &row) { row.host_base_ = 0u; }
    static void forget_length(SvmAllocation &row) { row.bytes_ = 0u; }
    static void a_pair_that_wraps(SvmAllocation &row)
    { row.host_base_ = ~std::uint64_t{0} - 8u; row.bytes_ = 64u; }
    static void stray_host_base(SvmAllocation &row) { row.host_base_ = 0x1000u; }
    static void stray_length(SvmAllocation &row) { row.bytes_ = 64u; }
    static void stray_map_depth(SvmAllocation &row) { row.map_depth_ = 1u; }
    static void pin_map_depth(SvmAllocation &row) { row.map_depth_ = ~std::uint32_t{0}; }
};

struct SvmTableProbe {
    static void miscount(SvmTable &table) { table.live_ += 1u; }
    static void count_past_the_table(SvmTable &table)
    { table.live_ = SvmTable::max_allocations + 1u; }
    static std::uint8_t *point_at_the_first_mirror(SvmTable &table)
    { std::uint8_t *const own = table.allocations_[1].mirror();
      table.allocations_[1].take(0x0000'7F55'0000'2000ull, table.allocations_[0].mirror(), 64u);
      return own; }
    static void restore_second_mirror(SvmTable &table, std::uint8_t *own)
    { table.allocations_[1].take(0x0000'7F55'0000'2000ull, own, 64u); }
    static void a_row_that_stands_for_nothing(SvmTable &table)
    { table.allocations_[0].take(0u, table.allocations_[0].mirror(), 64u); }
};

}

namespace {
constexpr std::uint64_t host_base = 0x0000'7F55'0000'1000ull;

}

TEST(svm_table, an_empty_table_translates_nothing)
{ MockPlatform platform;
  SvmTable table(platform);
  CHECK_EQ(table.live(), 0u);
  int elsewhere = 0;
  CHECK_EQ(table.host_address_of(&elsewhere), 0u);
  CHECK_EQ(table.host_address_of(nullptr), 0u);
  CHECK(table.covering(&elsewhere) == nullptr);
  CHECK(table.invariants_hold()); }

TEST(svm_table, an_allocation_translates_its_base_to_the_host_base)
{ MockPlatform platform;
  SvmTable table(platform);
  void *const mirror = table.take(host_base, 256u);
  CHECK(mirror != nullptr);
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.host_address_of(mirror), host_base);
  CHECK(table.invariants_hold()); }

TEST(svm_table, a_pointer_inside_translates_to_the_same_byte_on_the_host)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(table.take(host_base, 256u));
  CHECK(mirror != nullptr);
  CHECK_EQ(table.host_address_of(mirror + 1u), host_base + 1u);
  CHECK_EQ(table.host_address_of(mirror + 128u), host_base + 128u);
  CHECK_EQ(table.host_address_of(mirror + 255u), host_base + 255u); }

TEST(svm_table, one_past_the_end_is_not_inside)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(table.take(host_base, 256u));
  CHECK_EQ(table.host_address_of(mirror + 256u), 0u);
  CHECK_EQ(table.host_address_of(mirror - 1u), 0u); }

TEST(svm_table, an_allocation_given_back_translates_no_longer)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(table.take(host_base, 256u));
  CHECK_EQ(table.give_back(mirror), host_base);
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(table.host_address_of(mirror), 0u);
  CHECK(table.invariants_hold()); }

TEST(svm_table, freeing_takes_the_base_and_not_an_address_inside_it)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(table.take(host_base, 256u));
  CHECK_EQ(table.give_back(mirror + 8u), 0u);
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.give_back(nullptr), 0u);
  CHECK_EQ(table.give_back(mirror), host_base);
  CHECK_EQ(table.give_back(mirror), 0u);
  CHECK(table.invariants_hold()); }

TEST(svm_table, two_allocations_translate_independently)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const first = static_cast<std::uint8_t *>(table.take(host_base, 128u));
  std::uint8_t *const second = static_cast<std::uint8_t *>(table.take(host_base + 0x10000u, 128u));
  CHECK(first != nullptr);
  CHECK(second != nullptr);
  CHECK_EQ(table.host_address_of(first + 4u), host_base + 4u);
  CHECK_EQ(table.host_address_of(second + 4u), host_base + 0x10000u + 4u);
  CHECK_EQ(table.give_back(first), host_base);
  CHECK_EQ(table.host_address_of(second + 4u), host_base + 0x10000u + 4u);
  CHECK_EQ(table.host_address_of(first), 0u);
  CHECK(table.invariants_hold()); }

TEST(svm_table, an_allocation_of_nothing_or_at_nowhere_is_refused)
{ MockPlatform platform;
  SvmTable table(platform);
  CHECK(table.take(host_base, 0u) == nullptr);
  CHECK(table.take(0u, 256u) == nullptr);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

TEST(svm_table, a_pair_whose_translation_would_wrap_is_refused)
{ MockPlatform platform;
  SvmTable table(platform);
  CHECK(table.take(~std::uint64_t{0} - 15u, 64u) == nullptr);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

TEST(svm_table, a_table_with_no_memory_for_a_mirror_allocates_nothing)
{ MockPlatform platform;
  SvmTable table(platform);
  platform.fail_allocation_at(1u);
  CHECK(table.take(host_base, 256u) == nullptr);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

TEST(svm_table, a_full_table_refuses_and_stays_usable)
{ MockPlatform platform;
  SvmTable table(platform);
  void *first = nullptr;
  for (std::uint32_t index = 0u; index < SvmTable::max_allocations; ++index) {
      void *const mirror = table.take(host_base + index * 0x1000u, 16u);
      CHECK(mirror != nullptr);
      if (index == 0u) first = mirror;
  }
  CHECK_EQ(table.live(), SvmTable::max_allocations);
  CHECK(table.take(host_base + 0x1000000u, 16u) == nullptr);
  CHECK(table.invariants_hold());
  table.give_back(first);
  CHECK(table.take(host_base + 0x1000000u, 16u) != nullptr);
  CHECK(table.invariants_hold()); }

TEST(svm_table, giving_everything_back_leaves_nothing_translating)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const first = static_cast<std::uint8_t *>(table.take(host_base, 64u));
  std::uint8_t *const second = static_cast<std::uint8_t *>(table.take(host_base + 0x1000u, 64u));
  table.give_back_all();
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(table.host_address_of(first), 0u);
  CHECK_EQ(table.host_address_of(second), 0u);
  CHECK(table.invariants_hold());
  CHECK(table.take(host_base, 64u) != nullptr);
  CHECK(table.invariants_hold()); }

TEST(svm_table, the_allocation_covering_a_pointer_is_what_a_map_needs)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(table.take(host_base, 256u));
  SvmAllocation *const holding = table.covering(mirror + 16u);
  CHECK(holding != nullptr);
  CHECK_EQ(holding->host_base(), host_base);
  CHECK_EQ(holding->bytes(), 256u);
  CHECK(holding->mirror() == mirror);
  CHECK(table.covering(mirror + 256u) == nullptr); }

TEST(svm_allocation, maps_nest_as_the_api_says_they_do)
{ SvmAllocation allocation;
  std::uint8_t bytes[64] = {};
  allocation.take(host_base, bytes, sizeof bytes);
  CHECK_EQ(allocation.map_depth(), 0u);
  CHECK(!allocation.unmapped());
  allocation.mapped();
  allocation.mapped();
  CHECK_EQ(allocation.map_depth(), 2u);
  CHECK(allocation.unmapped());
  CHECK(allocation.unmapped());
  CHECK(!allocation.unmapped());
  CHECK(allocation.invariants_hold()); }

TEST(svm_allocation, one_that_is_held_by_nothing_covers_nothing)
{ SvmAllocation allocation;
  CHECK(!allocation.held());
  int elsewhere = 0;
  CHECK(!allocation.covers(&elsewhere));
  CHECK_EQ(allocation.host_address_of(&elsewhere), 0u);
  CHECK(allocation.invariants_hold());
  std::uint8_t bytes[64] = {};
  allocation.take(host_base, bytes, sizeof bytes);
  CHECK(allocation.held());
  allocation.give_up();
  CHECK(!allocation.held());
  CHECK(!allocation.covers(bytes));
  CHECK(allocation.invariants_hold()); }

TEST(svm_table, the_dump_counts_what_is_live_and_what_is_mapped)
{ MockPlatform platform;
  SvmTable table(platform);
  char        bytes[256];
  StateWriter empty(bytes, sizeof bytes);
  table.report(empty);
  CHECK(std::strstr(empty.dump(), "live=0") != nullptr);
  CHECK(std::strstr(empty.dump(), "mapped=0") != nullptr);

  void *const first = table.take(host_base, 256u);
  void *const second = table.take(host_base + 0x10000u, 256u);
  CHECK(first != nullptr && second != nullptr);
  SvmAllocation *const covering = table.covering(first);
  CHECK(covering != nullptr);
  covering->mapped();
  StateWriter written(bytes, sizeof bytes);
  table.report(written);
  CHECK(std::strstr(written.dump(), "live=2") != nullptr);
  CHECK(std::strstr(written.dump(), "mapped=1") != nullptr);
  CHECK(table.invariants_hold()); }

TEST(svm_table, a_table_with_no_room_and_a_platform_with_no_memory_both_answer_nothing)
{ MockPlatform platform;
  SvmTable table(platform);
  for (std::uint32_t index = 0u; index < SvmTable::max_allocations; ++index)
      CHECK(table.take(host_base + (std::uint64_t{index} << 20u), 64u) != nullptr);
  CHECK_EQ(table.live(), SvmTable::max_allocations);
  CHECK(table.take(host_base + 0x8000'0000ull, 64u) == nullptr);
  CHECK_EQ(table.live(), SvmTable::max_allocations);
  CHECK(table.invariants_hold());

  MockPlatform starved;
  SvmTable     hungry(starved);
  starved.fail_allocation_at(1u);
  CHECK(hungry.take(host_base, 64u) == nullptr);
  CHECK_EQ(hungry.live(), 0u);
  CHECK(hungry.invariants_hold()); }

TEST(svm_table, the_invariant_catches_the_states_it_is_there_to_catch)
{ MockPlatform platform;
  SvmTable table(platform);
  CHECK(table.take(host_base, 64u) != nullptr);
  CHECK(table.take(host_base + 0x10000u, 64u) != nullptr);
  CHECK(table.invariants_hold());
  SvmTableProbe::miscount(table);
  CHECK(!table.invariants_hold());

  MockPlatform beyond_platform;
  SvmTable     beyond(beyond_platform);
  SvmTableProbe::count_past_the_table(beyond);
  CHECK(!beyond.invariants_hold());

  MockPlatform overlap_platform;
  SvmTable     overlapping(overlap_platform);
  CHECK(overlapping.take(host_base, 64u) != nullptr);
  CHECK(overlapping.take(host_base + 0x10000u, 64u) != nullptr);
  std::uint8_t *const own = SvmTableProbe::point_at_the_first_mirror(overlapping);
  CHECK(!overlapping.invariants_hold());
  SvmTableProbe::restore_second_mirror(overlapping, own);
  CHECK(overlapping.invariants_hold());

  MockPlatform rowless_platform;
  SvmTable     rowless(rowless_platform);
  CHECK(rowless.take(host_base, 64u) != nullptr);
  SvmTableProbe::a_row_that_stands_for_nothing(rowless);
  CHECK(!rowless.invariants_hold()); }

TEST(svm_table, an_allocations_invariant_catches_a_pair_that_is_only_half_there)
{ const auto held_but_broken = [](void (*corrupt)(SvmAllocation &)) {
      SvmAllocation row;
      std::uint8_t  mirror[64] = {};
      row.take(host_base, mirror, sizeof mirror);
      CHECK(row.invariants_hold());
      corrupt(row);
      return row.invariants_hold();
  };
  CHECK(!held_but_broken(SvmAllocationProbe::forget_host_base));
  CHECK(!held_but_broken(SvmAllocationProbe::forget_length));
  CHECK(!held_but_broken(SvmAllocationProbe::a_pair_that_wraps));

  const auto empty_but_naming = [](void (*corrupt)(SvmAllocation &)) {
      SvmAllocation row;
      CHECK(row.invariants_hold());
      corrupt(row);
      return row.invariants_hold();
  };
  CHECK(!empty_but_naming(SvmAllocationProbe::stray_host_base));
  CHECK(!empty_but_naming(SvmAllocationProbe::stray_length));
  CHECK(!empty_but_naming(SvmAllocationProbe::stray_map_depth)); }

TEST(svm_table, a_map_depth_that_cannot_rise_any_further_stays_where_it_is)
{ SvmAllocation row;
  std::uint8_t  mirror[64] = {};
  row.take(host_base, mirror, sizeof mirror);
  SvmAllocationProbe::pin_map_depth(row);
  CHECK_EQ(row.map_depth(), ~std::uint32_t{0});
  row.mapped();
  CHECK_EQ(row.map_depth(), ~std::uint32_t{0});
  CHECK(row.invariants_hold()); }
