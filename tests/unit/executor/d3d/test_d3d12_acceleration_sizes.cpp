#include "executor/d3d/d3d12_acceleration_sizes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d12AccelerationSizes;
using av::MockPlatform;

TEST(d3d12_acceleration_sizes_note, keeps_the_structures_sorted_and_replaces_a_size_noted_again)
{ MockPlatform platform;
  D3d12AccelerationSizes sizes(platform);
  CHECK(sizes.note(0x3000u, 300u));
  CHECK(sizes.note(0x1000u, 100u));
  CHECK(sizes.note(0x2000u, 200u));
  CHECK(sizes.note(0x2000u, 250u));
  CHECK_EQ(sizes.count(), 3u);
  std::uint64_t bytes = 0u;
  CHECK(sizes.find(0x2000u, bytes));
  CHECK(bytes == 250u);
  CHECK(sizes.invariants_hold()); }

TEST(d3d12_acceleration_sizes_note, refuses_a_structure_with_no_address_or_no_bytes)
{ MockPlatform platform;
  D3d12AccelerationSizes sizes(platform);
  CHECK(!sizes.note(0u, 100u));
  CHECK(!sizes.note(0x1000u, 0u));
  CHECK_EQ(sizes.count(), 0u); }

TEST(d3d12_acceleration_sizes_find, knows_only_a_structure_that_was_built_at_exactly_that_address)
{ MockPlatform platform;
  D3d12AccelerationSizes sizes(platform);
  CHECK(sizes.note(0x1000u, 100u));
  std::uint64_t bytes = 7u;
  CHECK(!sizes.find(0x1100u, bytes));
  CHECK(!sizes.find(0xF00u, bytes));
  CHECK(!sizes.find(0x1001u, bytes));
  CHECK(bytes == 7u); }

TEST(d3d12_acceleration_sizes_clear, forgets_every_structure)
{ MockPlatform platform;
  D3d12AccelerationSizes sizes(platform);
  CHECK(sizes.note(0x1000u, 100u));
  sizes.clear();
  std::uint64_t bytes = 0u;
  CHECK_EQ(sizes.count(), 0u);
  CHECK(!sizes.find(0x1000u, bytes)); }
