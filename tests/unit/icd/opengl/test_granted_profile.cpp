#include "harness/framework/test_registry.hpp"
#include "icd/opengl/granted_profile.hpp"

#include <cstdint>

using av::glicd::GrantedProfile;
using Rung = GrantedProfile::Rung;

namespace av::glicd {

struct GrantedProfileProbe {
    static void set_mask(GrantedProfile &profile, std::uint32_t mask) { profile.mask_ = mask; }
};

}

TEST(granted_profile_invariants, a_mask_no_profile_defines_is_caught)
{ GrantedProfile profile;
  av::glicd::GrantedProfileProbe::set_mask(profile, 0x3u);
  CHECK(!profile.invariants_hold());
  av::glicd::GrantedProfileProbe::set_mask(profile, GrantedProfile::core_bit);
  CHECK(profile.invariants_hold()); }

TEST(granted_profile, a_context_nobody_asked_about_grants_nothing)
{ GrantedProfile profile;
  CHECK(profile.invariants_hold());
  CHECK(profile.rung() == Rung::none);
  CHECK_EQ(profile.mask(), 0u);
  CHECK_EQ(profile.major(), 0u);
  CHECK_EQ(profile.minor(), 0u);
  CHECK(!profile.known());
  CHECK(!profile.legacy_calls_live()); }

TEST(granted_profile, a_compatibility_context_runs_the_legacy_calls)
{ GrantedProfile profile;
  profile.granted(Rung::compatibility_46);
  profile.learn(4u, 6u, GrantedProfile::compatibility_bit);
  CHECK(profile.invariants_hold());
  CHECK(profile.rung() == Rung::compatibility_46);
  CHECK_EQ(profile.mask(), 2u);
  CHECK_EQ(profile.major(), 4u);
  CHECK_EQ(profile.minor(), 6u);
  CHECK(profile.known());
  CHECK(profile.legacy_calls_live()); }

TEST(granted_profile, a_core_context_refuses_them)
{ GrantedProfile profile;
  profile.granted(Rung::core_46);
  profile.learn(4u, 6u, GrantedProfile::core_bit);
  CHECK(profile.invariants_hold());
  CHECK_EQ(profile.mask(), 1u);
  CHECK(profile.known());
  CHECK(!profile.legacy_calls_live()); }

TEST(granted_profile, a_pre_profile_default_context_runs_them_too)
{ GrantedProfile profile;
  profile.granted(Rung::host_default);
  profile.learn(1u, 0u, 0u);
  CHECK(profile.invariants_hold());
  CHECK_EQ(profile.mask(), 0u);
  CHECK(profile.known());
  CHECK(profile.legacy_calls_live());
  GrantedProfile modern;
  modern.granted(Rung::host_default);
  modern.learn(3u, 2u, 0u);
  CHECK(modern.known());
  CHECK(!modern.legacy_calls_live()); }

TEST(granted_profile, a_mask_no_profile_defines_is_no_profile_at_all)
{ const std::uint32_t nonsense[] = {0x3u, 0x4u, 0xFFFFFFFFu, 0x80000002u};
  for (const std::uint32_t mask : nonsense) {
      GrantedProfile profile;
      profile.granted(Rung::compatibility_46);
      profile.learn(4u, 6u, mask);
      CHECK(profile.invariants_hold());
      CHECK_EQ(profile.mask(), 0u);
      CHECK(!profile.legacy_calls_live());
  }
  GrantedProfile future;
  future.granted(Rung::compatibility_46);
  future.learn(99u, 99u, GrantedProfile::compatibility_bit);
  CHECK(future.invariants_hold());
  CHECK_EQ(future.major(), 99u);
  CHECK(future.legacy_calls_live()); }

TEST(granted_profile, a_host_that_answered_nothing_unlocks_nothing)
{ GrantedProfile profile;
  profile.granted(Rung::host_default);
  profile.learn(0u, 0u, 0u);
  CHECK(profile.invariants_hold());
  CHECK(!profile.known());
  CHECK(!profile.legacy_calls_live()); }

TEST(granted_profile, the_last_answer_is_the_one_kept)
{ GrantedProfile profile;
  profile.granted(Rung::compatibility_46);
  profile.learn(4u, 6u, GrantedProfile::compatibility_bit);
  profile.granted(Rung::core_46);
  profile.learn(3u, 3u, GrantedProfile::core_bit);
  CHECK(profile.invariants_hold());
  CHECK(profile.rung() == Rung::core_46);
  CHECK_EQ(profile.major(), 3u);
  CHECK_EQ(profile.minor(), 3u);
  CHECK(!profile.legacy_calls_live()); }
