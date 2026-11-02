#include "miniport/ddi/dxgk_pte_record.hpp"
#include "miniport/memory/page_table_entry.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::DxgkPteRecord;
using av::PageTableEntry;

namespace {
constexpr std::uint64_t valid = 1u, zero = 2u, cache_coherent = 4u, read_only = 8u, no_execute = 16u;

}

TEST(page_table_entry_encoded, keeps_the_frame_segment_and_access_in_the_device_layout)
{ const DxgkPteRecord pte = {valid | cache_coherent | read_only | no_execute | (2u << 5u), 0x12345u};
  CHECK_EQ(PageTableEntry(pte).encoded(), (0x12345ull << 12u) | 0x1u | 0x2u | 0x4u | 0x8u | (2u << 4u)); }

TEST(page_table_entry_encoded, only_the_valid_bit_is_required)
{ const DxgkPteRecord pte = {valid, 7u};
  CHECK_EQ(PageTableEntry(pte).encoded(), (7ull << 12u) | 1u); }

TEST(page_table_entry_encoded, an_invalid_entry_is_zero_whatever_else_it_holds)
{ const DxgkPteRecord pte = {zero | read_only | (31u << 5u), 0xFFFFFu};
  CHECK_EQ(PageTableEntry(pte).encoded(), 0u); }

TEST(page_table_entry_encoded, bits_the_device_format_has_no_room_for_are_dropped)
{ const DxgkPteRecord pte = {valid | zero | (1u << 10u) | (0x3Full << 11u) | (0xFFull << 20u), 0xFFFFFFFFFFFFFFFFull};
  CHECK_EQ(PageTableEntry(pte).encoded(), 0xFFFFFFFFFFFFF001ull); }
