#include "harness/framework/test_registry.hpp"
#include "icd/d3d/d3d12_address_space.hpp"

#include <cstdint>

using av::d3d::D3d12AddressSpace;

TEST(d3d12_address_space, each_reservation_starts_where_the_last_one_ended_rounded_up)
{ D3d12AddressSpace space;
  const std::uint64_t first = space.reserve(1u);
  CHECK_EQ(first, D3d12AddressSpace::first);
  const std::uint64_t second = space.reserve(D3d12AddressSpace::alignment + 1u);
  CHECK_EQ(second, first + D3d12AddressSpace::alignment);
  const std::uint64_t third = space.reserve(16u);
  CHECK_EQ(third, second + 2u * D3d12AddressSpace::alignment);
  CHECK_EQ(space.reserved(), 4u * D3d12AddressSpace::alignment);
  CHECK(space.invariants_hold()); }

TEST(d3d12_address_space, no_bytes_and_more_than_the_space_holds_reserve_nothing)
{ D3d12AddressSpace space;
  CHECK_EQ(space.reserve(0u), 0u);
  const std::uint64_t end = std::uint64_t{1} << D3d12AddressSpace::address_bits;
  CHECK_EQ(space.reserve(end + 1u), 0u);
  CHECK_EQ(space.reserved(), 0u);
  const std::uint64_t rest = end - D3d12AddressSpace::first;
  CHECK_EQ(space.reserve(rest), D3d12AddressSpace::first);
  CHECK_EQ(space.reserve(1u), 0u);
  CHECK_EQ(space.reserved(), rest);
  CHECK(space.invariants_hold()); }
