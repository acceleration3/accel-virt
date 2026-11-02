#include "core/support/status.hpp"
#include "device/renderer/guest_memory_table.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GuestMemoryTable;
using av::Status;

namespace av {

struct GuestMemoryTableProbe {
    static void set_base(GuestMemoryTable &table, std::uint32_t slot, std::uintptr_t base)
    {
        table.regions_[slot].base_ = base;
    }
    static void set_id(GuestMemoryTable &table, std::uint32_t slot, std::uint32_t id) { table.regions_[slot].id_ = id; }
};

}

namespace {

constexpr std::uint64_t shift = GuestMemoryTable::region_shift;

std::uint64_t encode(std::uint64_t region, std::uint64_t offset) { return region << shift | offset; }

void *at(std::uintptr_t address) { return reinterpret_cast<void *>(address); }

}

TEST(guest_memory_table_add, a_region_resolves_and_locates_back_to_its_descriptor)
{ GuestMemoryTable table;
  std::uint8_t low[0x2000] = {};
  std::uint8_t high[0x1000] = {};
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.add(0u, low, sizeof low, 7), Status::ok);
  CHECK_STATUS(table.add(3u, high, sizeof high, 9), Status::ok);
  CHECK_EQ(table.count(), 2u);
  CHECK(table.invariants_hold());
  CHECK(table.resolve(encode(0u, 0x10u), 0x20u) == low + 0x10u);
  CHECK(table.resolve(encode(3u, 0xFFFu), 1u) == high + 0xFFFu);
  CHECK(table.resolve(encode(0u, 0u), sizeof low) == low);
  int           descriptor = -1;
  std::uint64_t offset = 0u;
  CHECK_STATUS(table.locate(high + 0x800u, 0x800u, descriptor, offset), Status::ok);
  CHECK_EQ(descriptor, 9);
  CHECK_EQ(offset, 0x800u);
  CHECK_STATUS(table.locate(low, sizeof low, descriptor, offset), Status::ok);
  CHECK_EQ(descriptor, 7);
  CHECK_EQ(offset, 0u); }

TEST(guest_memory_table_add, what_the_encoding_cannot_carry_or_a_malformed_region_is_refused)
{ GuestMemoryTable table;
  std::uint8_t page[0x1000] = {};
  CHECK_STATUS(table.add(0u, nullptr, 0x1000u, 3), Status::invalid_parameter);
  CHECK_STATUS(table.add(0u, page, 0u, 3), Status::invalid_parameter);
  CHECK_STATUS(table.add(0u, page, sizeof page, -1), Status::invalid_parameter);
  CHECK_STATUS(table.add(GuestMemoryTable::region_limit, page, sizeof page, 3), Status::invalid_parameter);
  CHECK_STATUS(table.add(0u, at(0x10000u), GuestMemoryTable::max_region_size + 1u, 3), Status::invalid_parameter);
  CHECK_STATUS(table.add(0u, at(UINTPTR_MAX - 0xFFFu), 0x1000u, 3), Status::invalid_parameter);
  CHECK_EQ(table.count(), 0u);
  CHECK_STATUS(table.add(0u, at(UINTPTR_MAX - 0x1000u), 0x1000u, 3), Status::ok);
  CHECK_STATUS(table.add(GuestMemoryTable::region_limit - 1u, page, sizeof page, 4), Status::ok);
  CHECK(table.resolve(encode(GuestMemoryTable::region_limit - 1u, 0x40u), 4u) == page + 0x40u);
  CHECK(table.invariants_hold()); }

TEST(guest_memory_table_add, a_region_as_large_as_an_offset_reaches_is_whole)
{ GuestMemoryTable table;
  const std::uintptr_t base = 0x100000000000u;
  CHECK_STATUS(table.add(1u, at(base), GuestMemoryTable::max_region_size, 5), Status::ok);
  CHECK(table.resolve(encode(1u, GuestMemoryTable::max_region_size - 1u), 1u) ==
        at(base + GuestMemoryTable::max_region_size - 1u));
  CHECK(table.resolve(encode(1u, 0u), GuestMemoryTable::max_region_size) == at(base));
  CHECK(table.resolve(encode(1u, 1u), GuestMemoryTable::max_region_size) == nullptr);
  CHECK(table.invariants_hold()); }

TEST(guest_memory_table_add, a_duplicate_id_conflicts_and_overlapping_ranges_are_refused)
{ GuestMemoryTable table;
  CHECK_STATUS(table.add(1u, at(0x10000u), 0x2000u, 3), Status::ok);
  CHECK_STATUS(table.add(1u, at(0x80000u), 0x1000u, 4), Status::conflict);
  CHECK_STATUS(table.add(1u, at(0x10000u), 0x2000u, 3), Status::conflict);
  CHECK_STATUS(table.add(2u, at(0x11000u), 0x2000u, 4), Status::invalid_parameter);
  CHECK_STATUS(table.add(2u, at(0x0F000u), 0x2000u, 4), Status::invalid_parameter);
  CHECK_STATUS(table.add(2u, at(0x08000u), 0x10000u, 4), Status::invalid_parameter);
  CHECK_STATUS(table.add(2u, at(0x0F000u), 0x1000u, 4), Status::ok);
  CHECK_STATUS(table.add(3u, at(0x12000u), 0x1000u, 5), Status::ok);
  CHECK_EQ(table.count(), 3u);
  CHECK(table.invariants_hold()); }

TEST(guest_memory_table_add, a_full_table_is_out_of_space)
{ GuestMemoryTable table;
  for (std::uint32_t region = 0u; region < GuestMemoryTable::max_regions; region++)
      CHECK_STATUS(table.add(region, at(0x100000u * (region + 1u)), 0x1000u, static_cast<int>(region)), Status::ok);
  CHECK_STATUS(table.add(99u, at(0x40000000u), 0x1000u, 99), Status::no_space);
  CHECK_EQ(table.count(), GuestMemoryTable::max_regions);
  CHECK(table.invariants_hold()); }

TEST(guest_memory_table_remove, hands_back_what_add_was_given_and_frees_the_slot)
{ GuestMemoryTable table;
  void         *base = nullptr;
  std::uint64_t size = 0u;
  int           descriptor = -1;
  CHECK_STATUS(table.remove(1u, base, size, descriptor), Status::not_found);
  CHECK(base == nullptr);
  CHECK_STATUS(table.add(1u, at(0x10000u), 0x2000u, 11), Status::ok);
  CHECK_STATUS(table.add(2u, at(0x20000u), 0x1000u, 12), Status::ok);
  CHECK_STATUS(table.remove(1u, base, size, descriptor), Status::ok);
  CHECK(base == at(0x10000u));
  CHECK_EQ(size, 0x2000u);
  CHECK_EQ(descriptor, 11);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.invariants_hold());
  CHECK(table.resolve(encode(1u, 0u), 1u) == nullptr);
  CHECK(table.resolve(encode(2u, 0u), 1u) == at(0x20000u));
  CHECK_STATUS(table.remove(1u, base, size, descriptor), Status::not_found);
  CHECK_STATUS(table.add(1u, at(0x10000u), 0x1000u, 13), Status::ok);
  CHECK(table.invariants_hold()); }

TEST(guest_memory_table_resolve, nothing_outside_one_region_resolves)
{ GuestMemoryTable table;
  CHECK(table.resolve(encode(0u, 0u), 1u) == nullptr);
  CHECK_STATUS(table.add(0u, at(0x10000u), 0x1000u, 3), Status::ok);
  CHECK_STATUS(table.add(1u, at(0x11000u), 0x1000u, 4), Status::ok);
  CHECK(table.resolve(encode(2u, 0u), 1u) == nullptr);
  CHECK(table.resolve(encode(0u, 0x10u), 0u) == nullptr);
  CHECK(table.resolve(encode(0u, 0x1000u), 1u) == nullptr);
  CHECK(table.resolve(encode(0u, 0x1001u), 0u) == nullptr);
  CHECK(table.resolve(encode(0u, 0x2000u), 1u) == nullptr);
  CHECK(table.resolve(encode(0u, 0xFFFu), 2u) == nullptr);
  CHECK(table.resolve(encode(0u, 0x10u), UINT64_MAX) == nullptr);
  CHECK(table.resolve(encode(0u, 0xFFFFFFFFFFu), 1u) == nullptr);
  CHECK(table.resolve(UINT64_MAX, 1u) == nullptr);
  CHECK(table.resolve(encode(0u, 0xFFFu), 1u) == at(0x10FFFu));
  CHECK(table.resolve(encode(1u, 0u), 0x1000u) == at(0x11000u)); }

TEST(guest_memory_table_locate, a_pointer_outside_every_region_or_spanning_two_is_not_found)
{ GuestMemoryTable table;
  int           descriptor = -1;
  std::uint64_t offset = 77u;
  CHECK_STATUS(table.locate(at(0x10000u), 1u, descriptor, offset), Status::not_found);
  CHECK_STATUS(table.add(0u, at(0x10000u), 0x1000u, 3), Status::ok);
  CHECK_STATUS(table.add(1u, at(0x11000u), 0x1000u, 4), Status::ok);
  CHECK_STATUS(table.locate(nullptr, 1u, descriptor, offset), Status::not_found);
  CHECK_STATUS(table.locate(at(0xFFFFu), 1u, descriptor, offset), Status::not_found);
  CHECK_STATUS(table.locate(at(0x12000u), 1u, descriptor, offset), Status::not_found);
  CHECK_STATUS(table.locate(at(0x10800u), 0u, descriptor, offset), Status::not_found);
  CHECK_STATUS(table.locate(at(0x10800u), 0x1000u, descriptor, offset), Status::not_found);
  CHECK_STATUS(table.locate(at(0x10800u), UINT64_MAX, descriptor, offset), Status::not_found);
  CHECK_EQ(descriptor, -1);
  CHECK_EQ(offset, 77u);
  CHECK_STATUS(table.locate(at(0x11FFFu), 1u, descriptor, offset), Status::ok);
  CHECK_EQ(descriptor, 4);
  CHECK_EQ(offset, 0xFFFu);
  CHECK_STATUS(table.locate(at(0x10000u), 0x1000u, descriptor, offset), Status::ok);
  CHECK_EQ(descriptor, 3);
  CHECK_EQ(offset, 0u); }

TEST(guest_memory_table_invariants, a_malformed_duplicate_or_overlapping_region_is_detected)
{ GuestMemoryTable malformed;
  CHECK_STATUS(malformed.add(0u, at(0x10000u), 0x1000u, 3), Status::ok);
  av::GuestMemoryTableProbe::set_base(malformed, 0u, 0u);
  CHECK(!malformed.invariants_hold());

  GuestMemoryTable duplicate;
  CHECK_STATUS(duplicate.add(0u, at(0x10000u), 0x1000u, 3), Status::ok);
  CHECK_STATUS(duplicate.add(1u, at(0x20000u), 0x1000u, 4), Status::ok);
  CHECK(duplicate.invariants_hold());
  av::GuestMemoryTableProbe::set_id(duplicate, 1u, 0u);
  CHECK(!duplicate.invariants_hold());

  GuestMemoryTable later;
  CHECK_STATUS(later.add(0u, at(0x10000u), 0x2000u, 3), Status::ok);
  CHECK_STATUS(later.add(1u, at(0x20000u), 0x1000u, 4), Status::ok);
  av::GuestMemoryTableProbe::set_base(later, 1u, 0x11000u);
  CHECK(!later.invariants_hold());

  GuestMemoryTable earlier;
  CHECK_STATUS(earlier.add(0u, at(0x10000u), 0x2000u, 3), Status::ok);
  CHECK_STATUS(earlier.add(1u, at(0x20000u), 0x2000u, 4), Status::ok);
  av::GuestMemoryTableProbe::set_base(earlier, 1u, 0xF000u);
  CHECK(!earlier.invariants_hold());
  av::GuestMemoryTableProbe::set_base(earlier, 1u, 0xE000u);
  CHECK(earlier.invariants_hold());

  GuestMemoryTable gap;
  CHECK_STATUS(gap.add(0u, at(0x10000u), 0x1000u, 3), Status::ok);
  CHECK_STATUS(gap.add(1u, at(0x20000u), 0x1000u, 4), Status::ok);
  CHECK_STATUS(gap.add(2u, at(0x30000u), 0x1000u, 5), Status::ok);
  void         *base = nullptr;
  std::uint64_t size = 0u;
  int           descriptor = -1;
  CHECK_STATUS(gap.remove(1u, base, size, descriptor), Status::ok);
  CHECK(gap.invariants_hold()); }
