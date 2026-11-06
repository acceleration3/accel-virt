#include "harness/framework/test_registry.hpp"
#include "icd/d3d/d3d12_address_space.hpp"

#include <cstdint>
#include <thread>
#include <vector>

using av::d3d::D3d12AddressSpace;

namespace av::d3d {

struct D3d12AddressSpaceProbe {
    static void set_next(D3d12AddressSpace &space, std::uint64_t next) { space.next_ = next; }
};

}

using av::d3d::D3d12AddressSpaceProbe;

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

TEST(d3d12_address_space, concurrent_reservations_never_overlap)
{ D3d12AddressSpace space;
  constexpr unsigned threads = 4u;
  constexpr unsigned each = 20000u;
  std::vector<std::vector<std::uint64_t>> bases(threads);
  std::vector<std::thread> workers;
  for (unsigned worker = 0u; worker < threads; worker++)
      workers.emplace_back([&space, &bases, worker] {
          for (unsigned count = 0u; count < each; count++) bases[worker].push_back(space.reserve(1u));
      });
  for (std::thread &worker : workers) worker.join();
  CHECK_EQ(space.reserved(), std::uint64_t{threads} * each * D3d12AddressSpace::alignment);
  std::vector<bool> seen(threads * each, false);
  bool distinct = true;
  for (const std::vector<std::uint64_t> &list : bases)
      for (std::uint64_t base : list) {
          const std::uint64_t slot = (base - D3d12AddressSpace::first) / D3d12AddressSpace::alignment;
          if (slot >= seen.size() || seen[slot]) distinct = false; else seen[slot] = true;
      }
  CHECK(distinct);
  CHECK(space.invariants_hold()); }

TEST(d3d12_address_space, a_next_outside_the_space_or_off_the_alignment_breaks_the_invariants)
{ D3d12AddressSpace space;
  CHECK(space.invariants_hold());
  D3d12AddressSpaceProbe::set_next(space, D3d12AddressSpace::first - D3d12AddressSpace::alignment);
  CHECK(!space.invariants_hold());
  D3d12AddressSpaceProbe::set_next(space, (std::uint64_t{1} << D3d12AddressSpace::address_bits) + D3d12AddressSpace::alignment);
  CHECK(!space.invariants_hold());
  D3d12AddressSpaceProbe::set_next(space, D3d12AddressSpace::first + 8u);
  CHECK(!space.invariants_hold());
  D3d12AddressSpaceProbe::set_next(space, std::uint64_t{1} << D3d12AddressSpace::address_bits);
  CHECK(space.invariants_hold()); }
