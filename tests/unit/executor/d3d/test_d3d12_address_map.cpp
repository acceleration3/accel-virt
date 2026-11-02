#include "executor/d3d/d3d12_address_map.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d12AddressMap;
using av::MockPlatform;
using av::Status;

TEST(d3d12_address_map_translate, an_address_inside_a_range_is_the_host_buffer_plus_its_offset)
{ MockPlatform platform;
  D3d12AddressMap map(platform);
  CHECK_STATUS(map.insert(0x10000u, 0x1000u, 0xA0000000u, 3u), Status::ok);
  CHECK(map.translate(0x10000u, 0u) == 0xA0000000u);
  CHECK(map.translate(0x10100u, 0x100u) == 0xA0000100u);
  CHECK(map.translate(0x10FFFu, 1u) == 0xA0000FFFu);
  CHECK(map.translate(0x10000u, 0x1000u) == 0xA0000000u);
  CHECK(map.translate(0x10001u, 0x1000u) == 0u);
  CHECK(map.translate(0x11000u, 0u) == 0u);
  CHECK(map.translate(0xFFFFu, 0u) == 0u);
  CHECK(map.invariants_hold()); }

TEST(d3d12_address_map_owner, names_the_buffer_and_the_offset_or_none)
{ MockPlatform platform;
  D3d12AddressMap map(platform);
  CHECK_STATUS(map.insert(0x2000u, 0x100u, 0x9000u, 11u), Status::ok);
  CHECK_STATUS(map.insert(0x1000u, 0x100u, 0x8000u, 10u), Status::ok);
  std::uint64_t offset = 77u;
  CHECK_EQ(map.owner(0x2040u, offset), 11u);
  CHECK(offset == 0x40u);
  CHECK_EQ(map.owner(0x10FFu, offset), 10u);
  CHECK(offset == 0xFFu);
  CHECK_EQ(map.owner(0x1100u, offset), 0u);
  CHECK(offset == 0u);
  CHECK_EQ(map.owner(0u, offset), 0u); }

TEST(d3d12_address_map_insert, refuses_an_empty_wrapping_overlapping_or_nameless_range)
{ MockPlatform platform;
  D3d12AddressMap map(platform);
  CHECK_STATUS(map.insert(0x4000u, 0x1000u, 0x1u, 1u), Status::ok);
  CHECK_STATUS(map.insert(0x8000u, 0u, 0x1u, 2u), Status::invalid_parameter);
  CHECK_STATUS(map.insert(~std::uint64_t{0} - 0xFFu, 0x100u, 0x1u, 2u), Status::invalid_parameter);
  CHECK_STATUS(map.insert(0x4800u, 0x10u, 0x1u, 2u), Status::conflict);
  CHECK_STATUS(map.insert(0x3000u, 0x1001u, 0x1u, 2u), Status::conflict);
  CHECK_STATUS(map.insert(0x4FFFu, 0x10u, 0x1u, 2u), Status::conflict);
  CHECK_STATUS(map.insert(0x3000u, 0x3000u, 0x1u, 2u), Status::conflict);
  CHECK_STATUS(map.insert(0x9000u, 0x10u, 0x1u, 0u), Status::invalid_parameter);
  CHECK_STATUS(map.insert(0x3000u, 0x1000u, 0x1u, 2u), Status::ok);
  CHECK_STATUS(map.insert(0x5000u, 0x1000u, 0x1u, 3u), Status::ok);
  CHECK_STATUS(map.insert(~std::uint64_t{0} - 0xFFu, 0xFFu, 0x1u, 4u), Status::ok);
  CHECK_EQ(map.count(), 4u);
  CHECK(map.invariants_hold()); }

TEST(d3d12_address_map_remove, forgets_one_buffer_s_range_and_keeps_the_rest_in_order)
{ MockPlatform platform;
  D3d12AddressMap map(platform);
  for (std::uint32_t id = 1u; id <= 5u; id++) CHECK_STATUS(map.insert(id * 0x1000u, 0x800u, id * 0x100000u, id), Status::ok);
  map.remove(3u);
  map.remove(99u);
  CHECK_EQ(map.count(), 4u);
  CHECK(map.translate(0x3000u, 0u) == 0u);
  CHECK(map.translate(0x4010u, 0u) == 0x400010u);
  CHECK(map.translate(0x1010u, 0u) == 0x100010u);
  CHECK(map.translate(0x5010u, 0u) == 0x500010u);
  CHECK_STATUS(map.insert(0x3000u, 0x1000u, 0x7u, 3u), Status::ok);
  CHECK(map.invariants_hold());
  map.clear();
  CHECK_EQ(map.count(), 0u);
  CHECK(map.translate(0x1010u, 0u) == 0u); }

TEST(d3d12_address_map_insert, holds_well_more_ranges_than_the_old_table_did)
{ MockPlatform platform;
  D3d12AddressMap map(platform);
  constexpr std::uint32_t ranges = 70000u;
  for (std::uint32_t id = 1u; id <= ranges; id++)
      CHECK_STATUS(map.insert(std::uint64_t{id} * 0x100u, 0x10u, std::uint64_t{id} << 20u, id), Status::ok);
  CHECK_EQ(map.count(), ranges);
  CHECK(map.invariants_hold());
  for (std::uint32_t id = 1u; id <= ranges; id += 997u) {
      std::uint64_t offset = 0u;
      CHECK(map.translate(std::uint64_t{id} * 0x100u + 4u, 4u) == (std::uint64_t{id} << 20u) + 4u);
      CHECK_EQ(map.owner(std::uint64_t{id} * 0x100u + 0xFu, offset), id);
      CHECK(offset == 0xFu);
  }
  map.remove(ranges / 2u);
  CHECK_EQ(map.count(), ranges - 1u);
  CHECK(map.translate(std::uint64_t{ranges / 2u} * 0x100u, 0u) == 0u);
  CHECK(map.invariants_hold()); }

TEST(d3d12_address_map_insert, a_range_that_finds_no_memory_changes_nothing)
{ MockPlatform platform;
  D3d12AddressMap map(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(map.insert(0x1000u, 0x100u, 0xA000u, 1u), Status::no_memory);
  CHECK_EQ(map.count(), 0u);
  CHECK(map.translate(0x1000u, 0u) == 0u);
  CHECK(map.invariants_hold());
  CHECK_STATUS(map.insert(0x1000u, 0x100u, 0xA000u, 1u), Status::ok);
  CHECK(map.translate(0x1010u, 0u) == 0xA010u); }
