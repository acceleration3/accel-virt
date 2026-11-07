#include "harness/framework/test_registry.hpp"
#include "proto/context/vram_policy.hpp"

#include <cstdint>

using av::VramPolicy;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

}

TEST(vram_policy, auto_reserves_an_eighth_of_the_card_and_never_less_than_a_gibibyte)
{ const VramPolicy policy;
  CHECK_EQ(policy.cap_for(16 * gib), 14 * gib);
  CHECK_EQ(policy.cap_for(8 * gib), 7 * gib);
  CHECK_EQ(policy.cap_for(4 * gib), 3 * gib);
  CHECK_EQ(policy.cap_for(2 * gib), 1 * gib);
  CHECK_EQ(policy.cap_for(1 * gib), gib / 2u);
  CHECK_EQ(policy.cap_for(24 * gib), 21 * gib);
  CHECK_EQ(policy.reserve_for(16 * gib), 2 * gib);
  CHECK_EQ(policy.reserve_for(4 * gib), gib); }

TEST(vram_policy, an_explicit_reserve_is_clamped_between_256_mib_and_the_card_less_512_mib)
{ const std::uint64_t mib = 1ull << 20;
  CHECK_EQ(VramPolicy(4096u).reserve_for(16 * gib), 4096 * mib);
  CHECK_EQ(VramPolicy(4096u).cap_for(16 * gib), 12 * gib);
  CHECK_EQ(VramPolicy(1u).reserve_for(16 * gib), 256 * mib);
  CHECK_EQ(VramPolicy(65536u).reserve_for(16 * gib), 16 * gib - 512 * mib);
  CHECK_EQ(VramPolicy(65536u).cap_for(16 * gib), 8 * gib);
  CHECK_EQ(VramPolicy(4096u).reserve_for(256 * mib), 0u);
  CHECK_EQ(VramPolicy(4096u).cap_for(256 * mib), 256 * mib); }
