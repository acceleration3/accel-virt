#include "executor/d3d/frame_claim.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <thread>

using av::FrameClaim;

TEST(frame_claim, a_frame_is_claimed_once_until_it_is_given_back)
{ FrameClaim claim;
  CHECK(!claim.out());
  CHECK(claim.claim());
  CHECK(claim.out());
  CHECK(!claim.claim());
  CHECK(claim.out());
  claim.give_back();
  CHECK(!claim.out());
  CHECK(claim.claim()); }

TEST(frame_claim, of_two_threads_claiming_at_once_exactly_one_wins)
{ constexpr std::uint32_t rounds = 2000u;
  for (std::uint32_t round = 0u; round < rounds; round++) {
      FrameClaim    claim;
      std::uint32_t won = 0u;
      std::thread   other([&] { if (claim.claim()) __atomic_add_fetch(&won, 1u, __ATOMIC_RELAXED); });
      if (claim.claim()) __atomic_add_fetch(&won, 1u, __ATOMIC_RELAXED);
      other.join();
      CHECK_EQ(won, 1u);
  } }
