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

namespace av {
struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_free_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.free_count_ = count; }
};
}

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
    static void miscount(SvmTable &table) { static_cast<void>(table.bases_.append(1u)); }
    static void shift_base(SvmTable &table, std::uint64_t by) { table.bases_.at(0u) += by; }
    static void overlap_the_next(SvmTable &table)
    { SvmAllocation &first = *table.allocations_.find(table.bases_.at(0u));
      first.take(first.host_base(), first.mirror(), std::size_t{1} << 30u); }
    static void restore_length(SvmTable &table, std::size_t bytes)
    { SvmAllocation &first = *table.allocations_.find(table.bases_.at(0u));
      first.take(first.host_base(), first.mirror(), bytes); }
    static void a_row_that_stands_for_nothing(SvmTable &table)
    { table.allocations_.find(table.bases_.at(0u))->give_up(); }
    static void a_row_that_is_half_there(SvmTable &table)
    { SvmAllocationProbe::forget_length(*table.allocations_.find(table.bases_.at(0u))); }
    static void miscount_the_slots(SvmTable &table, std::uint32_t free_count)
    { KeyedSlotsProbe::corrupt_free_count(table.allocations_, free_count); }
    static void resize_the_bases(SvmTable &table, std::uint32_t capacity)
    { GrowableArrayProbe::corrupt_capacity(table.bases_, capacity); }
    static void hold_again(SvmTable &table, std::uint64_t host, std::uint8_t *mirror, std::size_t bytes)
    { table.allocations_.find(table.bases_.at(0u))->take(host, mirror, bytes); }
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

TEST(svm_table, a_table_grows_past_any_fixed_bound_and_stays_usable_when_memory_runs_out)
{ MockPlatform platform;
  SvmTable table(platform);
  constexpr std::uint32_t many = 1000u;
  void *first = nullptr;
  for (std::uint32_t index = 0u; index < many; ++index) {
      void *const mirror = table.take(host_base + index * 0x1000u, 16u);
      CHECK(mirror != nullptr);
      if (index == 0u) first = mirror;
  }
  CHECK_EQ(table.live(), many);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.host_address_of(static_cast<std::uint8_t *>(first) + 3u), host_base + 3u);
  platform.fail_allocation_at(1u);
  CHECK(table.take(host_base + 0x1000000u, 16u) == nullptr);
  CHECK_EQ(table.live(), many);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.give_back(first), host_base);
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

TEST(svm_allocation, one_covers_the_bytes_of_its_mirror_and_nothing_beside_them)
{ SvmAllocation allocation;
  std::uint8_t    bytes[64] = {};
  allocation.take(host_base, bytes + 8, 32u);
  CHECK(allocation.covers(bytes + 8));
  CHECK(allocation.covers(bytes + 39));
  CHECK(!allocation.covers(bytes + 7));
  CHECK(!allocation.covers(bytes + 40));
  CHECK(!allocation.covers(nullptr));
  CHECK_EQ(allocation.host_address_of(bytes + 7), 0u);
  CHECK_EQ(allocation.host_address_of(bytes + 9), host_base + 1u); }

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

TEST(svm_table, a_platform_with_no_memory_answers_nothing_for_each_allocation_it_would_need)
{ for (std::uint32_t refused = 1u; refused <= 3u; ++refused) {
      MockPlatform starved;
      SvmTable     hungry(starved);
      starved.fail_allocation_at(refused);
      CHECK(hungry.take(host_base, 64u) == nullptr);
      CHECK_EQ(hungry.live(), 0u);
      CHECK(hungry.invariants_hold());
      CHECK(hungry.take(host_base, 64u) != nullptr);
      CHECK_EQ(hungry.live(), 1u);
      CHECK(hungry.invariants_hold());
  } }

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
  CHECK(beyond.take(host_base, 64u) != nullptr);
  SvmTableProbe::shift_base(beyond, 1u);
  CHECK(!beyond.invariants_hold());
  SvmTableProbe::shift_base(beyond, ~std::uint64_t{0});
  CHECK(beyond.invariants_hold());

  MockPlatform overlap_platform;
  SvmTable     overlapping(overlap_platform);
  CHECK(overlapping.take(host_base, 64u) != nullptr);
  CHECK(overlapping.take(host_base + 0x10000u, 64u) != nullptr);
  SvmTableProbe::overlap_the_next(overlapping);
  CHECK(!overlapping.invariants_hold());
  SvmTableProbe::restore_length(overlapping, 64u);
  CHECK(overlapping.invariants_hold());

  MockPlatform rowless_platform;
  SvmTable     rowless(rowless_platform);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(rowless.take(host_base, 64u));
  CHECK(mirror != nullptr);
  SvmTableProbe::a_row_that_stands_for_nothing(rowless);
  CHECK(!rowless.invariants_hold());
  SvmTableProbe::hold_again(rowless, host_base, mirror, 64u);
  CHECK(rowless.invariants_hold());

  MockPlatform half_platform;
  SvmTable     half(half_platform);
  CHECK(half.take(host_base, 64u) != nullptr);
  SvmTableProbe::a_row_that_is_half_there(half);
  CHECK(!half.invariants_hold());
  SvmTableProbe::restore_length(half, 64u);
  CHECK(half.invariants_hold());

  MockPlatform moved_platform;
  SvmTable     moved(moved_platform);
  std::uint8_t *const own = static_cast<std::uint8_t *>(moved.take(host_base, 64u));
  CHECK(own != nullptr);
  static std::uint8_t elsewhere[64];
  SvmTableProbe::hold_again(moved, host_base, elsewhere, 64u);
  CHECK(!moved.invariants_hold());
  SvmTableProbe::hold_again(moved, host_base, own, 64u);
  CHECK(moved.invariants_hold());

  MockPlatform slots_platform;
  SvmTable     slots(slots_platform);
  CHECK(slots.take(host_base, 64u) != nullptr);
  SvmTableProbe::miscount_the_slots(slots, 5u);
  CHECK(!slots.invariants_hold());
  SvmTableProbe::miscount_the_slots(slots, 0u);
  CHECK(slots.invariants_hold());

  MockPlatform bases_platform;
  SvmTable     bases(bases_platform);
  CHECK(bases.take(host_base, 64u) != nullptr);
  SvmTableProbe::resize_the_bases(bases, 12u);
  CHECK(!bases.invariants_hold());
  SvmTableProbe::resize_the_bases(bases, av::GrowableArray<std::uint64_t>::first_capacity);
  CHECK(bases.invariants_hold()); }

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
