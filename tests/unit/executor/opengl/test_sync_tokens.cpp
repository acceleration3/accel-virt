/* The host fences one context's guest tokens name. A GLsync is a pointer into
 * the driver, so what is asserted is that a token the host never bound names
 * nothing - which is what stops a guest handing the driver a pointer it chose -
 * that a retired token answers through the stand-in and never has a driver
 * object deleted twice, and that a guest which never deletes is served
 * without the table growing. */
#include "executor/opengl/sync_tokens.hpp"
#include "harness/executor/retired_sync_tokens_probe.hpp"
#include "harness/executor/sync_tokens_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::RetiredSyncTokensProbe;
using av::SyncTokens;
using av::SyncTokensProbe;

namespace {

int first = 0;
int second = 0;

/* As many distinct driver syncs as a table holds live, and one more. */
int syncs[SyncTokens::max_live + 1u];

/* Tokens 1 to max_live bound to syncs[0..], the table full. */
void fill(SyncTokens &tokens)
{
    for (std::uint32_t i = 0u; i < SyncTokens::max_live; i++) CHECK(tokens.bind(i + 1u, &syncs[i]));
    CHECK(tokens.full());
}

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
  CHECK(tokens.fence_for(6u) == nullptr);
  CHECK(tokens.fence_for(0u) == nullptr);
  CHECK(tokens.fence_for(~std::uint64_t{0}) == nullptr); }

/* Tokens are minted increasing and one context's fences arrive in order, so
 * a token at or below the newest bound is a reuse or an invention: refused
 * while live, which would lose the fence it named, and after it is deleted. */
TEST(sync_tokens, a_token_not_newer_than_the_newest_bound_is_refused)
{ SyncTokens tokens;
  CHECK(tokens.bind(7u, &first));
  CHECK(!tokens.bind(7u, &second));
  CHECK(!tokens.bind(6u, &second));
  CHECK(tokens.fence_for(7u) == &first);
  CHECK(tokens.fence_for(6u) == nullptr);
  CHECK(tokens.forget(7u) == &first);
  CHECK(!tokens.bind(7u, &second));
  CHECK(tokens.bind(8u, &second));
  CHECK_EQ(tokens.count(), 1u);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, a_zero_token_or_a_failed_fence_is_not_a_binding)
{ SyncTokens tokens;
  CHECK(!tokens.bind(0u, &first));
  CHECK(!tokens.bind(7u, nullptr));
  CHECK_EQ(tokens.count(), 0u);
  /* A failed fence moved nothing on: its token can still be bound. */
  CHECK(tokens.bind(7u, &first));
  CHECK(tokens.invariants_hold()); }

/* glDeleteSync deletes what forget answers, and only that. */
TEST(sync_tokens, forgetting_one_answers_its_fence_and_leaves_the_rest)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  CHECK(tokens.forget(1u) == &first);
  CHECK(tokens.invariants_hold());
  CHECK(tokens.fence_for(1u) == nullptr);
  CHECK(tokens.fence_for(2u) == &second);
  CHECK(tokens.forget(1u) == nullptr);
  CHECK(tokens.forget(9u) == nullptr);
  CHECK(tokens.forget(0u) == nullptr);
  CHECK_EQ(tokens.count(), 1u);
  CHECK(tokens.oldest() == &second);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, a_table_is_full_at_its_bound_and_refuses_more_until_one_goes)
{ SyncTokens tokens;
  CHECK(!tokens.full());
  CHECK(tokens.oldest() == nullptr);
  fill(tokens);
  CHECK_EQ(tokens.count(), SyncTokens::max_live);
  CHECK(tokens.oldest() == &syncs[0]);
  CHECK(!tokens.bind(SyncTokens::max_live + 1u, &syncs[SyncTokens::max_live]));
  CHECK(tokens.fence_for(SyncTokens::max_live + 1u) == nullptr);
  CHECK(tokens.forget(SyncTokens::max_live / 2u) == &syncs[SyncTokens::max_live / 2u - 1u]);
  CHECK(!tokens.full());
  CHECK(tokens.bind(SyncTokens::max_live + 1u, &syncs[SyncTokens::max_live]));
  CHECK(tokens.invariants_hold()); }

/* The first sync retired is kept as the stand-in and deleted by nobody yet;
 * every later one is handed back for the driver to delete. Each retired
 * token answers through the stand-in from then on. */
TEST(sync_tokens, retiring_keeps_the_first_as_the_stand_in_and_hands_back_the_rest)
{ SyncTokens tokens;
  fill(tokens);
  CHECK(tokens.retire_oldest() == nullptr);
  CHECK(tokens.invariants_hold());
  CHECK_EQ(tokens.count(), SyncTokens::max_live - 1u);
  CHECK(tokens.oldest() == &syncs[1]);
  CHECK(tokens.fence_for(1u) == &syncs[0]);
  CHECK(tokens.retire_oldest() == &syncs[1]);
  CHECK(tokens.invariants_hold());
  CHECK(tokens.fence_for(2u) == &syncs[0]);
  CHECK(tokens.fence_for(1u) == &syncs[0]);
  CHECK(tokens.fence_for(3u) == &syncs[2]);
  /* Room again, and only a newer token takes it. */
  CHECK(!tokens.bind(2u, &first));
  CHECK(tokens.bind(SyncTokens::max_live + 1u, &first));
  CHECK(tokens.fence_for(SyncTokens::max_live + 1u) == &first); }

TEST(sync_tokens, retiring_from_an_empty_table_does_nothing)
{ SyncTokens tokens;
  CHECK(tokens.retire_oldest() == nullptr);
  CHECK_EQ(tokens.count(), 0u);
  CHECK(tokens.fence_for(0u) == nullptr);
  CHECK(tokens.invariants_hold());
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.retire_oldest() == nullptr);
  /* Emptied by retiring, it retires nothing more and the stand-in stays. */
  CHECK(tokens.retire_oldest() == nullptr);
  CHECK(tokens.fence_for(1u) == &first);
  CHECK(tokens.invariants_hold()); }

/* glDeleteSync of a retired token deletes nothing - the stand-in answers for
 * the others - and the token names nothing after. */
TEST(sync_tokens, forgetting_a_retired_token_hands_back_nothing)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  CHECK(tokens.bind(3u, &syncs[0]));
  CHECK(tokens.retire_oldest() == nullptr);
  CHECK(tokens.retire_oldest() == &second);
  CHECK(tokens.forget(1u) == nullptr);
  CHECK(tokens.invariants_hold());
  CHECK(tokens.fence_for(1u) == nullptr);
  CHECK(tokens.fence_for(2u) == &first);
  CHECK(tokens.forget(2u) == nullptr);
  CHECK(tokens.fence_for(2u) == nullptr);
  CHECK(tokens.invariants_hold());
  /* Every retired token gone, the stand-in is still held, for closing. */
  void *fence = nullptr;
  CHECK(tokens.take_any(fence));
  CHECK(fence == &syncs[0]);
  CHECK(tokens.take_any(fence));
  CHECK(fence == &first); }

/* A closing context takes every driver sync back, each once and the stand-in
 * last, so it can delete them while it still has a context to delete them
 * with; then the table is as new. */
TEST(sync_tokens, take_any_gives_every_fence_back_once_and_the_stand_in_last)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &syncs[0]));
  CHECK(tokens.bind(2u, &first));
  CHECK(tokens.bind(3u, &second));
  CHECK(tokens.retire_oldest() == nullptr);
  void *fence = nullptr;
  bool  seen_first = false, seen_second = false;
  std::uint32_t taken = 0u;
  while (tokens.take_any(fence)) {
      CHECK(fence != nullptr);
      seen_first = seen_first || fence == &first;
      seen_second = seen_second || fence == &second;
      taken++;
      if (taken == 3u) CHECK(fence == &syncs[0]);
      CHECK(tokens.invariants_hold());
  }
  CHECK_EQ(taken, 3u);
  CHECK(seen_first && seen_second);
  CHECK(fence == nullptr);
  CHECK_EQ(tokens.count(), 0u);
  CHECK(tokens.fence_for(1u) == nullptr);
  CHECK(tokens.fence_for(2u) == nullptr);
  /* Emptied, it binds again from the start. */
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.fence_for(1u) == &first); }

TEST(sync_tokens, take_any_of_an_empty_table_takes_nothing)
{ SyncTokens tokens;
  void *fence = &first;
  CHECK(!tokens.take_any(fence));
  CHECK(fence == nullptr);
  CHECK(tokens.invariants_hold()); }

/* What DaVinci Resolve does: fence every frame, delete nothing, and keep
 * asking after each. Retiring the oldest whenever the table is full - the GPU
 * has passed it here - serves every token, with never more than the bound in
 * driver syncs and one run of retired tokens. */
TEST(sync_tokens, a_guest_that_never_deletes_is_served_without_growing)
{ SyncTokens tokens;
  constexpr std::uint64_t fences = 5000u;
  static char   made[fences];
  std::uint32_t handed_back = 0u;
  for (std::uint64_t token = 1u; token <= fences; token++) {
      if (tokens.full()) {
          void *const retired = tokens.retire_oldest();
          /* Handed back in the order they were made: the oldest live. */
          if (retired != nullptr) CHECK(retired == &made[token - SyncTokens::max_live - 1u]);
          handed_back += retired != nullptr ? 1u : 0u;
      }
      CHECK(tokens.bind(token, &made[token - 1u]));
      CHECK(tokens.invariants_hold());
      CHECK(tokens.count() <= SyncTokens::max_live);
  }
  /* Every token minted is answered: the newest by its own sync, the rest
   * through the stand-in, the first made. */
  const std::uint64_t retired = fences - SyncTokens::max_live;
  for (std::uint64_t token = 1u; token <= retired; token++) CHECK(tokens.fence_for(token) == &made[0]);
  for (std::uint64_t token = retired + 1u; token <= fences; token++)
      CHECK(tokens.fence_for(token) == &made[token - 1u]);
  CHECK(tokens.fence_for(fences + 1u) == nullptr);
  CHECK_EQ(tokens.count(), SyncTokens::max_live);
  /* All but the stand-in went back to the driver as they were retired. */
  CHECK_EQ(handed_back, fences - SyncTokens::max_live - 1u);
  CHECK_EQ(SyncTokensProbe(tokens).retired().runs(), 1u); }

/* A count past the table is one `fence_for` would walk off the end of, handing
 * the driver whatever lies beyond as a GLsync. Every slot is bound first, so
 * nothing but the count is wrong. */
TEST(sync_tokens_invariants, a_count_past_the_table_breaks_them)
{ SyncTokens tokens;
  fill(tokens);
  CHECK(tokens.invariants_hold());
  SyncTokensProbe(tokens).set_count(SyncTokens::max_live + 1u);
  CHECK(!tokens.invariants_hold()); }

/* bind refuses both of these, so a held one was never bound: a zero token is
 * the guest failing to mint one, a null fence is glFenceSync having failed. */
TEST(sync_tokens_invariants, a_zero_token_in_the_table_breaks_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  SyncTokensProbe(tokens).set_token(0u, 0u);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, a_null_fence_in_the_table_breaks_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  SyncTokensProbe(tokens).set_fence(1u, nullptr);
  CHECK(!tokens.invariants_hold()); }

/* Out of the order they were bound in, the oldest is no longer first, and a
 * full table would wait on and retire the wrong sync. */
TEST(sync_tokens_invariants, live_tokens_out_of_order_break_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  SyncTokensProbe(tokens).set_token(1u, 1u);
  CHECK(!tokens.invariants_hold()); }

/* A live token past the newest is one bind would let a later fence reuse. */
TEST(sync_tokens_invariants, a_live_token_past_the_newest_breaks_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(5u, &first));
  SyncTokensProbe(tokens).set_newest(4u);
  CHECK(!tokens.invariants_hold()); }

/* Retired tokens answer through the stand-in, so one with none would answer
 * null for a sync the guest still holds. */
TEST(sync_tokens_invariants, retired_tokens_with_no_stand_in_break_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.retire_oldest() == nullptr);
  CHECK(tokens.invariants_hold());
  SyncTokensProbe(tokens).set_stand_in(nullptr);
  CHECK(!tokens.invariants_hold()); }

/* A live slot holding the stand-in would have it deleted twice at close. */
TEST(sync_tokens_invariants, a_live_slot_holding_the_stand_in_breaks_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  CHECK(tokens.retire_oldest() == nullptr);
  SyncTokensProbe(tokens).set_fence(0u, &first);
  CHECK(!tokens.invariants_hold()); }

/* A token both live and retired: retirement takes the oldest live, so every
 * retired token is older than every live one. */
TEST(sync_tokens_invariants, a_live_token_not_newer_than_the_retired_breaks_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.bind(2u, &second));
  CHECK(tokens.retire_oldest() == nullptr);
  SyncTokensProbe(tokens).set_token(0u, 1u);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, broken_retired_tokens_break_them)
{ SyncTokens tokens;
  CHECK(tokens.bind(1u, &first));
  CHECK(tokens.retire_oldest() == nullptr);
  SyncTokensProbe probe(tokens);
  RetiredSyncTokensProbe(probe.retired()).set_run(0u, 0u, 1u);
  CHECK(!tokens.invariants_hold()); }
