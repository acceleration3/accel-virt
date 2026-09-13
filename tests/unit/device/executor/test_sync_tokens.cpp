/* The host fences one context's guest tokens name. A GLsync is a pointer into
 * the driver, so what is asserted is that a token the host never bound names
 * nothing - which is what stops a guest handing the driver a pointer it chose. */
#include "device/executor/sync_tokens.hpp"
#include "harness/executor/sync_tokens_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::SyncTokens;
using av::SyncTokensProbe;

namespace {

int first = 0;
int second = 0;

}  // namespace

TEST(sync_tokens, a_bound_token_names_the_fence_it_was_bound_to)
{ SyncTokens tokens;
  CHECK(tokens.fence_for(7u) == nullptr);
  CHECK(tokens.bind(7u, &first));
  CHECK(tokens.fence_for(7u) == &first);
  CHECK_EQ(tokens.count(), 1u);
  CHECK(tokens.invariants_hold()); }

/* The whole point: a number the guest made up is not a fence. */
TEST(sync_tokens, a_token_nobody_bound_names_nothing)
{ SyncTokens tokens;
  CHECK(tokens.bind(7u, &first));
  CHECK(tokens.fence_for(8u) == nullptr);
  CHECK(tokens.fence_for(0u) == nullptr);
  CHECK(tokens.fence_for(~std::uint64_t{0}) == nullptr); }

/* A guest reusing a live token would lose the fence it named, and the driver's
 * object with it. */
TEST(sync_tokens, a_token_already_bound_is_refused)
{ SyncTokens tokens;
  CHECK(tokens.bind(7u, &first));
  CHECK(!tokens.bind(7u, &second));
  CHECK(tokens.fence_for(7u) == &first);
  CHECK_EQ(tokens.count(), 1u); }

TEST(sync_tokens, a_zero_token_or_a_failed_fence_is_not_a_binding)
{ SyncTokens tokens;
  CHECK(!tokens.bind(0u, &first));
  CHECK(!tokens.bind(7u, nullptr));
  CHECK_EQ(tokens.count(), 0u);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, forgetting_one_leaves_the_rest)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  tokens.forget(1u);
  CHECK(tokens.fence_for(1u) == nullptr);
  CHECK(tokens.fence_for(2u) == &second);
  tokens.forget(9u);
  CHECK_EQ(tokens.count(), 1u);
  tokens.forget_all();
  CHECK_EQ(tokens.count(), 0u);
  CHECK(tokens.invariants_hold()); }

/* A guest that never deletes a fence has to be bounded somewhere. */
TEST(sync_tokens, a_guest_that_never_deletes_is_refused_not_served)
{ SyncTokens tokens;
  for (std::uint64_t token = 1u; token <= SyncTokens::max_tokens; token++)
      CHECK(tokens.bind(token, &first));
  CHECK_EQ(tokens.count(), SyncTokens::max_tokens);
  CHECK(!tokens.bind(SyncTokens::max_tokens + 1u, &second));
  CHECK(tokens.fence_for(1u) == &first);
  CHECK(tokens.invariants_hold()); }

/* A count past the table is one `fence_for` would walk off the end of, handing
 * the driver whatever lies beyond as a GLsync. Every slot is bound first, so
 * nothing but the count is wrong. */
TEST(sync_tokens_invariants, a_count_past_the_table_breaks_them)
{ SyncTokens tokens;
  for (std::uint64_t token = 1u; token <= SyncTokens::max_tokens; token++)
      CHECK(tokens.bind(token, &first));
  SyncTokensProbe(tokens).set_count(SyncTokens::max_tokens + 1u);
  CHECK(!tokens.invariants_hold()); }

/* bind refuses both of these, so a held one was never bound: a zero token is
 * the guest failing to mint one, a null fence is glFenceSync having failed. */
TEST(sync_tokens_invariants, a_zero_token_in_the_table_breaks_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  SyncTokensProbe(tokens).set_token(1u, 0u);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, a_null_fence_in_the_table_breaks_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  SyncTokensProbe(tokens).set_fence(1u, nullptr);
  CHECK(!tokens.invariants_hold()); }
