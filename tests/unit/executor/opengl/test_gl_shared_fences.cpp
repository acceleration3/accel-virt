#include "executor/opengl/gl_shared_fences.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GlSharedFences;

namespace {

int firsts = 0;
int seconds = 0;

}

TEST(gl_shared_fences, a_published_token_is_found_until_its_fence_is_withdrawn)
{ GlSharedFences fences;
  CHECK(fences.find(5u) == nullptr);
  fences.publish(5u, &firsts);
  fences.publish(6u, &seconds);
  CHECK(fences.find(5u) == &firsts);
  CHECK(fences.find(6u) == &seconds);
  CHECK_EQ(fences.count(), 2u);
  fences.withdraw(&firsts);
  CHECK(fences.find(5u) == nullptr);
  CHECK(fences.find(6u) == &seconds);
  fences.withdraw(nullptr);
  fences.withdraw(&firsts);
  CHECK_EQ(fences.count(), 1u);
  CHECK(fences.invariants_hold()); }

TEST(gl_shared_fences, nothing_is_published_without_a_token_and_a_fence)
{ GlSharedFences fences;
  fences.publish(0u, &firsts);
  fences.publish(3u, nullptr);
  CHECK_EQ(fences.count(), 0u);
  CHECK(fences.find(0u) == nullptr);
  CHECK(fences.invariants_hold()); }

TEST(gl_shared_fences, the_oldest_row_is_overwritten_when_the_table_is_full)
{ GlSharedFences fences;
  static int fence[GlSharedFences::most_fences + 1u];
  for (std::uint32_t at = 0u; at <= GlSharedFences::most_fences; at++) fences.publish(at + 1u, &fence[at]);
  CHECK_EQ(fences.count(), GlSharedFences::most_fences);
  CHECK(fences.find(1u) == nullptr);
  CHECK(fences.find(GlSharedFences::most_fences + 1u) == &fence[GlSharedFences::most_fences]);
  CHECK(fences.invariants_hold()); }

TEST(gl_shared_fences, a_token_past_the_newest_is_awaited_and_one_below_it_is_not)
{ GlSharedFences fences;
  fences.publish(9u, &firsts);
  CHECK(fences.await(5u, 1000000u) == nullptr);
  CHECK(fences.await(9u, 1000000u) == &firsts);
  CHECK(fences.await(12u, 1000000u) == nullptr);
  CHECK(fences.invariants_hold()); }
