#include "executor/d3d/flip_chains.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::FlipChains;

namespace {

bool lists(const std::uint64_t *found, std::uint32_t count, std::uint64_t surface)
{
    for (std::uint32_t index = 0u; index < count; index++)
        if (found[index] == surface) return true;
    return false;
}

}

TEST(flip_chains, the_surfaces_of_one_rotation_are_each_others_siblings)
{ FlipChains          chains;
  const std::uint64_t chain[] = {10u, 20u, 30u};
  chains.rotated(chain, 3u);
  CHECK(chains.invariants_hold());
  std::uint64_t found[FlipChains::member_capacity] = {};
  CHECK(chains.siblings(10u, found, FlipChains::member_capacity) == 2u);
  CHECK(lists(found, 2u, 20u));
  CHECK(lists(found, 2u, 30u));
  CHECK(!lists(found, 2u, 10u));
  CHECK(chains.siblings(30u, found, FlipChains::member_capacity) == 2u);
  CHECK(lists(found, 2u, 10u));
  CHECK(lists(found, 2u, 20u));
  CHECK(chains.siblings(40u, found, FlipChains::member_capacity) == 0u);
  CHECK(chains.siblings(0u, found, FlipChains::member_capacity) == 0u); }

TEST(flip_chains, rotating_the_same_buffers_again_keeps_one_chain)
{ FlipChains          chains;
  const std::uint64_t first[] = {10u, 20u};
  const std::uint64_t again[] = {20u, 10u};
  for (std::uint32_t chain = 0u; chain + 1u < FlipChains::chain_capacity; chain++) {
      const std::uint64_t other[] = {1000u + 2u * chain, 1001u + 2u * chain};
      chains.rotated(other, 2u);
  }
  chains.rotated(first, 2u);
  for (int present = 0; present < 100; present++) chains.rotated(present % 2 == 0 ? again : first, 2u);
  CHECK(chains.invariants_hold());
  std::uint64_t found[FlipChains::member_capacity] = {};
  CHECK(chains.siblings(10u, found, FlipChains::member_capacity) == 1u);
  CHECK(found[0] == 20u);
  CHECK(chains.siblings(1000u, found, FlipChains::member_capacity) == 1u);
  CHECK(found[0] == 1001u); }

TEST(flip_chains, a_rotation_that_names_an_unshared_or_repeated_surface_makes_no_chain)
{ FlipChains          chains;
  const std::uint64_t unshared[] = {10u, 0u};
  const std::uint64_t repeated[] = {10u, 10u};
  const std::uint64_t alone[] = {10u};
  std::uint64_t       many[FlipChains::member_capacity + 1u] = {};
  for (std::uint32_t index = 0u; index < FlipChains::member_capacity + 1u; index++) many[index] = 100u + index;
  chains.rotated(unshared, 2u);
  chains.rotated(repeated, 2u);
  chains.rotated(alone, 1u);
  chains.rotated(nullptr, 2u);
  chains.rotated(many, FlipChains::member_capacity + 1u);
  CHECK(chains.invariants_hold());
  std::uint64_t found[FlipChains::member_capacity] = {};
  CHECK(chains.siblings(10u, found, FlipChains::member_capacity) == 0u);
  CHECK(chains.siblings(100u, found, FlipChains::member_capacity) == 0u);
  CHECK(chains.siblings(10u, nullptr, FlipChains::member_capacity) == 0u); }

TEST(flip_chains, a_chain_left_with_one_buffer_ends)
{ FlipChains          chains;
  const std::uint64_t pair[] = {10u, 20u};
  const std::uint64_t taker[] = {20u, 30u};
  chains.rotated(pair, 2u);
  chains.rotated(taker, 2u);
  CHECK(chains.invariants_hold());
  std::uint64_t found[FlipChains::member_capacity] = {};
  CHECK(chains.siblings(10u, found, FlipChains::member_capacity) == 0u);
  CHECK(chains.siblings(30u, found, FlipChains::member_capacity) == 1u);
  CHECK(found[0] == 20u); }

TEST(flip_chains, buffers_rotated_into_a_new_chain_leave_their_old_one)
{ FlipChains          chains;
  const std::uint64_t old_chain[] = {10u, 20u};
  const std::uint64_t new_chain[] = {20u, 30u, 40u};
  chains.rotated(old_chain, 2u);
  chains.rotated(new_chain, 3u);
  CHECK(chains.invariants_hold());
  std::uint64_t found[FlipChains::member_capacity] = {};
  CHECK(chains.siblings(10u, found, FlipChains::member_capacity) == 0u);
  CHECK(chains.siblings(20u, found, FlipChains::member_capacity) == 2u);
  CHECK(lists(found, 2u, 30u));
  CHECK(lists(found, 2u, 40u));
  const std::uint64_t grown[] = {20u, 30u};
  chains.rotated(grown, 2u);
  CHECK(chains.invariants_hold());
  CHECK(chains.siblings(40u, found, FlipChains::member_capacity) == 0u);
  CHECK(chains.siblings(30u, found, FlipChains::member_capacity) == 1u);
  CHECK(found[0] == 20u); }

TEST(flip_chains, a_full_table_gives_the_least_recently_rotated_chain_up_and_siblings_fill_only_the_room_given)
{ FlipChains chains;
  for (std::uint32_t chain = 0u; chain < FlipChains::chain_capacity; chain++) {
      const std::uint64_t buffers[] = {1000u + 3u * chain, 1001u + 3u * chain, 1002u + 3u * chain};
      chains.rotated(buffers, 3u);
  }
  const std::uint64_t first[] = {1001u, 1002u, 1000u};
  chains.rotated(first, 3u);
  const std::uint64_t late[] = {5u, 6u};
  chains.rotated(late, 2u);
  CHECK(chains.invariants_hold());
  std::uint64_t found[FlipChains::member_capacity] = {};
  CHECK(chains.siblings(5u, found, FlipChains::member_capacity) == 1u);
  CHECK(found[0] == 6u);
  CHECK(chains.siblings(1003u, found, FlipChains::member_capacity) == 0u);
  CHECK(chains.siblings(1006u, found, FlipChains::member_capacity) == 2u);
  CHECK(chains.siblings(1000u, found, 1u) == 1u);
  CHECK(found[0] == 1001u || found[0] == 1002u);
  CHECK(chains.siblings(1000u, found, 0u) == 0u); }
