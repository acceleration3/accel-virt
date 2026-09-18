/* The mirrors an application holds, and the host addresses they stand for.
 *
 * A kernel may only dereference a pointer the host's own clSVMAlloc produced,
 * and the guest cannot touch that memory, so an allocation is a pair: the host
 * address, which is what crosses the wire, and a mirror the application is
 * handed. This is the only place one becomes the other.
 *
 * What matters: a pointer inside a live allocation translates to the same byte
 * on the host, a pointer inside no live allocation translates to nothing, and
 * freeing takes the base the application was given rather than any address
 * inside it. Coarse-grained is what makes the pair correct rather than a
 * compromise - host access is defined only between map and unmap, so there is
 * no moment the two are entitled to disagree. */
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/svm_table.hpp"

#include <cstdint>

using av::MockPlatform;
using av::clicd::SvmAllocation;
using av::clicd::SvmTable;

namespace {

/* A host address far from anything this process owns, so a bug that used it as
 * a pointer would crash rather than pass. */
constexpr std::uint64_t host_base = 0x0000'7F55'0000'1000ull;

}  // namespace

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
  /* This is why a kernel argument may point into the middle of an allocation:
   * the offset is what is preserved. */
  CHECK_EQ(table.host_address_of(mirror + 1u), host_base + 1u);
  CHECK_EQ(table.host_address_of(mirror + 128u), host_base + 128u);
  CHECK_EQ(table.host_address_of(mirror + 255u), host_base + 255u); }

TEST(svm_table, one_past_the_end_is_not_inside)
{ MockPlatform platform;
  SvmTable table(platform);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(table.take(host_base, 256u));
  /* It is also the first byte of whatever the allocator put next. */
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
  /* clSVMFree takes what clSVMAlloc answered. An address inside the
   * allocation is a pointer the host's allocator never handed out. */
  CHECK_EQ(table.give_back(mirror + 8u), 0u);
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.give_back(nullptr), 0u);
  CHECK_EQ(table.give_back(mirror), host_base);
  /* And twice over frees nothing, as applications do. */
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
  /* Freeing one leaves the other. */
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
  /* A host base near the end of the address space with a size that would run
   * past it: the arithmetic is refused here rather than wrapping later. */
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
  /* And it still works afterwards. */
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
  /* An unmap with nothing mapped is the application's mistake, not a count
   * that goes below zero. */
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
