#include "harness/framework/test_registry.hpp"
#include "proto/context/host_ram_policy.hpp"

#include <cstdint>

using av::HostRamPolicy;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

}

TEST(host_ram_policy, auto_keeps_a_quarter_of_the_host_and_never_less_than_four_gibibytes_for_the_desktop)
{ const HostRamPolicy policy;
  CHECK_EQ(policy.reserve_for(30 * gib), 7 * gib + gib / 2u);
  CHECK_EQ(policy.reserve_for(64 * gib), 16 * gib);
  CHECK_EQ(policy.reserve_for(8 * gib), 4 * gib);
  CHECK_EQ(policy.reserve_for(16 * gib), 4 * gib); }

TEST(host_ram_policy, the_budget_is_what_the_guest_ram_and_the_reserve_leave_of_the_host)
{ const HostRamPolicy policy;
  CHECK_EQ(policy.budget_for(30 * gib, 9 * gib), 13 * gib + gib / 2u);
  CHECK_EQ(policy.budget_for(64 * gib, 8 * gib), 40 * gib);
  CHECK_EQ(policy.budget_for(16 * gib, 4 * gib), 8 * gib); }

TEST(host_ram_policy, a_host_with_nothing_left_still_gets_the_least_budget_not_zero_which_would_mean_unlimited)
{ const HostRamPolicy policy;
  CHECK_EQ(policy.budget_for(8 * gib, 6 * gib), HostRamPolicy::least_budget);
  CHECK_EQ(policy.budget_for(8 * gib, 100 * gib), HostRamPolicy::least_budget);
  CHECK_EQ(policy.budget_for(2 * gib, 0u), HostRamPolicy::least_budget); }

TEST(host_ram_policy, a_host_whose_memory_is_unknown_has_no_budget)
{ const HostRamPolicy policy;
  CHECK_EQ(policy.budget_for(0u, 4 * gib), 0u); }

TEST(host_ram_policy, an_explicit_reserve_replaces_the_auto_one_and_is_never_below_a_gibibyte)
{ CHECK_EQ(HostRamPolicy(2048u).reserve_for(30 * gib), 2 * gib);
  CHECK_EQ(HostRamPolicy(2048u).budget_for(30 * gib, 9 * gib), 19 * gib);
  CHECK_EQ(HostRamPolicy(1u).reserve_for(30 * gib), gib);
  CHECK_EQ(HostRamPolicy(16384u).budget_for(30 * gib, 9 * gib), 5 * gib);
  CHECK_EQ(HostRamPolicy(2048u).reserve_mib(), 2048u);
  CHECK_EQ(HostRamPolicy().reserve_mib(), 0u); }

TEST(host_ram_policy, the_auto_floor_is_the_reserve_but_never_more_than_half_of_what_the_guest_leaves_nor_less_than_one_gibibyte)
{ const HostRamPolicy policy;
  CHECK_EQ(policy.floor_for(30 * gib, 8 * gib), 7 * gib + gib / 2u);
  CHECK_EQ(policy.floor_for(30 * gib, 16 * gib), 7 * gib);
  CHECK_EQ(policy.floor_for(30 * gib, 28 * gib), gib);
  CHECK_EQ(policy.floor_for(8 * gib, 100 * gib), gib);
  CHECK_EQ(policy.floor_for(64 * gib, 8 * gib), 16 * gib);
  CHECK_EQ(policy.floor_for(0u, 4 * gib), 0u); }

TEST(host_ram_policy, an_explicit_floor_is_what_was_asked_not_clamped_by_the_guest)
{ CHECK_EQ(HostRamPolicy(2048u).floor_for(30 * gib, 28 * gib), 2 * gib);
  CHECK_EQ(HostRamPolicy(16384u).floor_for(30 * gib, 16 * gib), 16 * gib);
  CHECK_EQ(HostRamPolicy(1u).floor_for(30 * gib, 1 * gib), gib); }
