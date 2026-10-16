/* The tokens of syncs the host saw signaled and let the driver's object go
 * for. What is asserted is that membership stays exact while the runs fit -
 * a token this context never retired, or deleted, answers nothing - and that
 * the state held never grows with the fences a guest leaves behind. */
#include "executor/opengl/retired_sync_tokens.hpp"
#include "harness/executor/retired_sync_tokens_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::RetiredSyncTokens;
using av::RetiredSyncTokensProbe;

namespace {

constexpr std::uint64_t last_token = ~std::uint64_t{0};

/* Every other token from `first`: one context's fences with another's
 * between each pair, one run apiece. */
void add_interleaved(RetiredSyncTokens &retired, std::uint64_t first, std::uint32_t count)
{
    for (std::uint32_t i = 0u; i < count; i++) {
        retired.add(first + 2u * i);
        CHECK(retired.invariants_hold());
    }
}

}  // namespace

TEST(retired_sync_tokens, starts_holding_nothing)
{ RetiredSyncTokens retired;
  CHECK_EQ(retired.runs(), 0u);
  CHECK_EQ(retired.newest(), 0u);
  CHECK(!retired.contains(0u));
  CHECK(!retired.contains(1u));
  CHECK(retired.invariants_hold()); }

/* The leak this exists for: a context fencing alone and deleting nothing
 * holds one run, however many fences it makes. */
TEST(retired_sync_tokens, a_context_fencing_alone_holds_one_run_for_every_token)
{ RetiredSyncTokens retired;
  constexpr std::uint64_t fences = 10000u;
  for (std::uint64_t token = 1u; token <= fences; token++) {
      retired.add(token);
      CHECK(retired.invariants_hold());
      CHECK_EQ(retired.runs(), 1u);
  }
  CHECK(retired.contains(1u));
  CHECK(retired.contains(fences / 2u));
  CHECK(retired.contains(fences));
  CHECK(!retired.contains(fences + 1u));
  CHECK(!retired.contains(0u));
  CHECK_EQ(retired.newest(), fences); }

/* Retirements come in the order tokens were minted; anything else - a
 * repeat, an older one, zero - is not one, and changes nothing. */
TEST(retired_sync_tokens, a_token_not_newer_than_every_one_held_is_not_added)
{ RetiredSyncTokens retired;
  retired.add(0u);
  CHECK_EQ(retired.runs(), 0u);
  retired.add(5u);
  retired.add(5u);
  retired.add(3u);
  retired.add(0u);
  CHECK_EQ(retired.runs(), 1u);
  CHECK(!retired.contains(3u));
  CHECK(!retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK_EQ(retired.newest(), 5u);
  CHECK(retired.invariants_hold()); }

/* Another context's tokens between this one's are not this one's. */
TEST(retired_sync_tokens, a_token_between_two_runs_is_not_held)
{ RetiredSyncTokens retired;
  add_interleaved(retired, 1u, 3u);
  CHECK_EQ(retired.runs(), 3u);
  CHECK(retired.contains(1u));
  CHECK(!retired.contains(2u));
  CHECK(retired.contains(3u));
  CHECK(!retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK(!retired.contains(6u)); }

/* The bound: past max_runs the two oldest join, and the token between them -
 * the oldest gap - is the only one that comes to be held unbound. Every newer
 * gap stays exact. */
TEST(retired_sync_tokens, past_its_runs_the_oldest_two_join)
{ RetiredSyncTokens retired;
  add_interleaved(retired, 1u, RetiredSyncTokens::max_runs);
  CHECK_EQ(retired.runs(), RetiredSyncTokens::max_runs);
  CHECK(!retired.contains(2u));
  const std::uint64_t next = 1u + 2u * RetiredSyncTokens::max_runs;
  retired.add(next);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), RetiredSyncTokens::max_runs);
  CHECK(retired.contains(2u));
  CHECK(!retired.contains(4u));
  CHECK(!retired.contains(next - 1u));
  CHECK(retired.contains(next));
  CHECK_EQ(retired.newest(), next);
  /* However many more, the runs never grow past the bound. */
  add_interleaved(retired, next + 2u, 1000u);
  CHECK_EQ(retired.runs(), RetiredSyncTokens::max_runs);
  CHECK(!retired.contains(next + 2u * 1000u - 1u)); }

TEST(retired_sync_tokens, removing_a_lone_token_closes_its_run)
{ RetiredSyncTokens retired;
  add_interleaved(retired, 1u, 3u);
  retired.remove(3u);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 2u);
  CHECK(!retired.contains(3u));
  CHECK(retired.contains(1u));
  CHECK(retired.contains(5u)); }

TEST(retired_sync_tokens, removing_either_end_of_a_run_shortens_it)
{ RetiredSyncTokens retired;
  for (std::uint64_t token = 1u; token <= 4u; token++) retired.add(token);
  retired.remove(1u);
  CHECK(retired.invariants_hold());
  CHECK(!retired.contains(1u));
  CHECK(retired.contains(2u));
  retired.remove(4u);
  CHECK(retired.invariants_hold());
  CHECK(!retired.contains(4u));
  CHECK(retired.contains(3u));
  CHECK_EQ(retired.newest(), 3u);
  CHECK_EQ(retired.runs(), 1u); }

/* A deleted sync names nothing after, even inside a run. */
TEST(retired_sync_tokens, removing_from_inside_a_run_splits_it)
{ RetiredSyncTokens retired;
  for (std::uint64_t token = 1u; token <= 5u; token++) retired.add(token);
  retired.add(9u);
  retired.remove(3u);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 3u);
  CHECK(retired.contains(2u));
  CHECK(!retired.contains(3u));
  CHECK(retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK(retired.contains(9u));
  /* The split keeps the runs in order: a later add still extends the newest. */
  retired.add(10u);
  CHECK_EQ(retired.runs(), 3u);
  CHECK(retired.invariants_hold()); }

/* With every run in use there is no room to split, and the deleted token
 * stays held rather than another being let go. */
TEST(retired_sync_tokens, a_full_set_keeps_a_token_it_cannot_split_out)
{ RetiredSyncTokens retired;
  add_interleaved(retired, 1u, RetiredSyncTokens::max_runs - 1u);
  const std::uint64_t run = 1u + 2u * (RetiredSyncTokens::max_runs - 1u);
  for (std::uint64_t token = run; token <= run + 2u; token++) retired.add(token);
  CHECK_EQ(retired.runs(), RetiredSyncTokens::max_runs);
  retired.remove(run + 1u);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), RetiredSyncTokens::max_runs);
  CHECK(retired.contains(run + 1u));
  /* Its ends can still go. */
  retired.remove(run + 2u);
  CHECK(!retired.contains(run + 2u)); }

TEST(retired_sync_tokens, removing_a_token_not_held_changes_nothing)
{ RetiredSyncTokens retired;
  retired.add(4u);
  retired.add(5u);
  retired.remove(0u);
  retired.remove(3u);
  retired.remove(6u);
  retired.remove(last_token);
  CHECK_EQ(retired.runs(), 1u);
  CHECK(retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK(retired.invariants_hold()); }

TEST(retired_sync_tokens, clearing_holds_nothing_and_starts_over)
{ RetiredSyncTokens retired;
  add_interleaved(retired, 10u, 4u);
  retired.clear();
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 0u);
  CHECK_EQ(retired.newest(), 0u);
  CHECK(!retired.contains(10u));
  retired.add(1u);
  CHECK(retired.contains(1u)); }

/* A guest's token can be any 64-bit value; the largest still holds, ends
 * a run, and nothing can follow it. */
TEST(retired_sync_tokens, the_largest_token_is_held_and_ends_everything)
{ RetiredSyncTokens retired;
  retired.add(last_token - 1u);
  retired.add(last_token);
  CHECK_EQ(retired.runs(), 1u);
  CHECK(retired.contains(last_token));
  CHECK_EQ(retired.newest(), last_token);
  retired.add(1u);
  CHECK(!retired.contains(1u));
  CHECK(retired.invariants_hold());
  retired.remove(last_token);
  CHECK(!retired.contains(last_token));
  CHECK(retired.contains(last_token - 1u)); }

TEST(retired_sync_tokens_invariants, a_count_past_the_runs_breaks_them)
{ RetiredSyncTokens retired;
  add_interleaved(retired, 1u, RetiredSyncTokens::max_runs);
  RetiredSyncTokensProbe(retired).set_count(RetiredSyncTokens::max_runs + 1u);
  CHECK(!retired.invariants_hold()); }

/* Zero is no token, so a run starting there holds one nobody minted. */
TEST(retired_sync_tokens_invariants, a_run_from_zero_breaks_them)
{ RetiredSyncTokens retired;
  retired.add(1u);
  RetiredSyncTokensProbe(retired).set_run(0u, 0u, 1u);
  CHECK(!retired.invariants_hold()); }

TEST(retired_sync_tokens_invariants, a_run_ending_before_it_starts_breaks_them)
{ RetiredSyncTokens retired;
  retired.add(5u);
  RetiredSyncTokensProbe(retired).set_run(0u, 5u, 4u);
  CHECK(!retired.invariants_hold()); }

/* Runs a token apart: touching, they were one; out of order, add would
 * extend the wrong one and contains answer for tokens never added. */
TEST(retired_sync_tokens_invariants, runs_touching_or_out_of_order_break_them)
{ { RetiredSyncTokens retired;
    add_interleaved(retired, 1u, 2u);
    RetiredSyncTokensProbe(retired).set_run(1u, 2u, 3u);
    CHECK(!retired.invariants_hold()); }
  { RetiredSyncTokens retired;
    add_interleaved(retired, 1u, 2u);
    RetiredSyncTokensProbe(retired).set_run(0u, 7u, 7u);
    CHECK(!retired.invariants_hold()); } }

/* Nothing follows the largest token, so a run after one ending there is one
 * the arithmetic of "a token apart" would wrap around to accept. */
TEST(retired_sync_tokens_invariants, a_run_after_the_largest_token_breaks_them)
{ RetiredSyncTokens retired;
  add_interleaved(retired, 1u, 2u);
  RetiredSyncTokensProbe probe(retired);
  probe.set_run(0u, last_token - 1u, last_token);
  probe.set_run(1u, 1u, 1u);
  CHECK(!retired.invariants_hold()); }
