/* The tokens of syncs the host saw signaled and let the driver's object go
 * for. What is asserted is that membership stays exact while there is room -
 * a token this context never retired, or deleted, answers nothing - and that
 * a context fencing alone holds one run however many fences it leaves. */
#include "executor/opengl/retired_sync_tokens.hpp"
#include "harness/executor/retired_sync_tokens_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::RetiredSyncTokens;
using av::RetiredSyncTokensProbe;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::uint64_t last_token = ~std::uint64_t{0};

/* Every other token from `first`: one context's fences with another's
 * between each pair, one run apiece. */
void add_interleaved(RetiredSyncTokens &retired, std::uint64_t first, std::uint32_t count)
{
    for (std::uint32_t i = 0u; i < count; i++) {
        CHECK(retired.add(first + 2u * i) == Status::ok);
        CHECK(retired.invariants_hold());
    }
}

}  // namespace

TEST(retired_sync_tokens, starts_holding_nothing)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  CHECK_EQ(retired.runs(), 0u);
  CHECK_EQ(retired.newest(), 0u);
  CHECK(!retired.contains(0u));
  CHECK(!retired.contains(1u));
  CHECK(retired.invariants_hold()); }

/* The leak this exists for: a context fencing alone and deleting nothing
 * holds one run, however many fences it makes. */
TEST(retired_sync_tokens, a_context_fencing_alone_holds_one_run_for_every_token)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  constexpr std::uint64_t fences = 10000u;
  for (std::uint64_t token = 1u; token <= fences; token++) {
      CHECK(retired.add(token) == Status::ok);
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
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  CHECK(retired.add(0u) == Status::ok);
  CHECK_EQ(retired.runs(), 0u);
  CHECK(retired.add(5u) == Status::ok);
  CHECK(retired.add(5u) == Status::ok);
  CHECK(retired.add(3u) == Status::ok);
  CHECK(retired.add(0u) == Status::ok);
  CHECK_EQ(retired.runs(), 1u);
  CHECK(!retired.contains(3u));
  CHECK(!retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK_EQ(retired.newest(), 5u);
  CHECK(retired.invariants_hold()); }

/* Another context's tokens between this one's are not this one's. */
TEST(retired_sync_tokens, a_token_between_two_runs_is_not_held)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  add_interleaved(retired, 1u, 3u);
  CHECK_EQ(retired.runs(), 3u);
  CHECK(retired.contains(1u));
  CHECK(!retired.contains(2u));
  CHECK(retired.contains(3u));
  CHECK(!retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK(!retired.contains(6u)); }

/* Well past the 64 runs a context once held before joining its oldest:
 * another context's tokens between every pair of this one's, and every gap
 * stays exact. */
TEST(retired_sync_tokens, interleaved_tokens_keep_every_gap_exact)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  add_interleaved(retired, 1u, 10000u);
  CHECK_EQ(retired.runs(), 10000u);
  bool exact = true;
  for (std::uint64_t token = 1u; token <= 20000u; token++) exact = exact && retired.contains(token) == (token % 2u == 1u);
  CHECK(exact); }

/* With no room for another run the two oldest join, and the token between
 * them - the oldest gap - is the only one that comes to be held unbound. With
 * no room even for a first run, the token is not held at all. */
TEST(retired_sync_tokens, with_no_room_for_a_run_the_oldest_two_join)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  add_interleaved(retired, 1u, 8u);
  CHECK(!retired.contains(2u));
  platform.fail_allocation_at(1u);
  CHECK(retired.add(17u) == Status::ok);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 8u);
  CHECK(retired.contains(2u));
  CHECK(!retired.contains(4u));
  CHECK(!retired.contains(16u));
  CHECK(retired.contains(17u));
  CHECK_EQ(retired.newest(), 17u);
  RetiredSyncTokens empty(platform);
  platform.fail_allocation_at(1u);
  CHECK(empty.add(5u) == Status::no_memory);
  CHECK(!empty.contains(5u));
  CHECK_EQ(empty.runs(), 0u);
  RetiredSyncTokens lone(platform);
  CHECK(lone.add(5u) == Status::ok);
  for (std::uint64_t token = 7u; token < 21u; token += 2u) CHECK(lone.add(token) == Status::ok);
  platform.fail_allocation_at(1u);
  CHECK(lone.add(30u) == Status::ok);
  CHECK(lone.contains(6u));
  CHECK(lone.invariants_hold()); }

TEST(retired_sync_tokens, removing_a_lone_token_closes_its_run)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  add_interleaved(retired, 1u, 3u);
  retired.remove(3u);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 2u);
  CHECK(!retired.contains(3u));
  CHECK(retired.contains(1u));
  CHECK(retired.contains(5u)); }

TEST(retired_sync_tokens, removing_either_end_of_a_run_shortens_it)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  for (std::uint64_t token = 1u; token <= 4u; token++) CHECK(retired.add(token) == Status::ok);
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
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  for (std::uint64_t token = 1u; token <= 5u; token++) CHECK(retired.add(token) == Status::ok);
  CHECK(retired.add(9u) == Status::ok);
  retired.remove(3u);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 3u);
  CHECK(retired.contains(2u));
  CHECK(!retired.contains(3u));
  CHECK(retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK(retired.contains(9u));
  /* The split keeps the runs in order: a later add still extends the newest. */
  CHECK(retired.add(10u) == Status::ok);
  CHECK_EQ(retired.runs(), 3u);
  CHECK(retired.invariants_hold()); }

/* With no room to split, the deleted token stays held rather than another
 * being let go. */
TEST(retired_sync_tokens, a_set_that_cannot_grow_keeps_a_token_it_cannot_split_out)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  add_interleaved(retired, 1u, 7u);
  const std::uint64_t run = 15u;
  for (std::uint64_t token = run; token <= run + 2u; token++) CHECK(retired.add(token) == Status::ok);
  CHECK_EQ(retired.runs(), 8u);
  platform.fail_allocation_at(1u);
  retired.remove(run + 1u);
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 8u);
  CHECK(retired.contains(run + 1u));
  /* Its ends can still go. */
  retired.remove(run + 2u);
  CHECK(!retired.contains(run + 2u)); }

TEST(retired_sync_tokens, removing_a_token_not_held_changes_nothing)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  CHECK(retired.add(4u) == Status::ok);
  CHECK(retired.add(5u) == Status::ok);
  retired.remove(0u);
  retired.remove(3u);
  retired.remove(6u);
  retired.remove(last_token);
  CHECK_EQ(retired.runs(), 1u);
  CHECK(retired.contains(4u));
  CHECK(retired.contains(5u));
  CHECK(retired.invariants_hold()); }

TEST(retired_sync_tokens, clearing_holds_nothing_and_starts_over)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  add_interleaved(retired, 10u, 4u);
  retired.clear();
  CHECK(retired.invariants_hold());
  CHECK_EQ(retired.runs(), 0u);
  CHECK_EQ(retired.newest(), 0u);
  CHECK(!retired.contains(10u));
  CHECK(retired.add(1u) == Status::ok);
  CHECK(retired.contains(1u)); }

/* A guest's token can be any 64-bit value; the largest still holds, ends
 * a run, and nothing can follow it. */
TEST(retired_sync_tokens, the_largest_token_is_held_and_ends_everything)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  CHECK(retired.add(last_token - 1u) == Status::ok);
  CHECK(retired.add(last_token) == Status::ok);
  CHECK_EQ(retired.runs(), 1u);
  CHECK(retired.contains(last_token));
  CHECK_EQ(retired.newest(), last_token);
  CHECK(retired.add(1u) == Status::ok);
  CHECK(!retired.contains(1u));
  CHECK(retired.invariants_hold());
  retired.remove(last_token);
  CHECK(!retired.contains(last_token));
  CHECK(retired.contains(last_token - 1u)); }

/* Zero is no token, so a run starting there holds one nobody minted. */
TEST(retired_sync_tokens_invariants, a_run_from_zero_breaks_them)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  CHECK(retired.add(1u) == Status::ok);
  RetiredSyncTokensProbe(retired).set_run(0u, 0u, 1u);
  CHECK(!retired.invariants_hold()); }

TEST(retired_sync_tokens_invariants, a_run_ending_before_it_starts_breaks_them)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  CHECK(retired.add(5u) == Status::ok);
  RetiredSyncTokensProbe(retired).set_run(0u, 5u, 4u);
  CHECK(!retired.invariants_hold()); }

/* Runs a token apart: touching, they were one; out of order, add would
 * extend the wrong one and contains answer for tokens never added. */
TEST(retired_sync_tokens_invariants, runs_touching_or_out_of_order_break_them)
{ { MockPlatform      platform;
  RetiredSyncTokens retired(platform);
    add_interleaved(retired, 1u, 2u);
    RetiredSyncTokensProbe(retired).set_run(1u, 2u, 3u);
    CHECK(!retired.invariants_hold()); }
  { MockPlatform      platform;
  RetiredSyncTokens retired(platform);
    add_interleaved(retired, 1u, 2u);
    RetiredSyncTokensProbe(retired).set_run(0u, 7u, 7u);
    CHECK(!retired.invariants_hold()); } }

/* Nothing follows the largest token, so a run after one ending there is one
 * the arithmetic of "a token apart" would wrap around to accept. */
TEST(retired_sync_tokens_invariants, a_run_after_the_largest_token_breaks_them)
{ MockPlatform      platform;
  RetiredSyncTokens retired(platform);
  add_interleaved(retired, 1u, 2u);
  RetiredSyncTokensProbe probe(retired);
  probe.set_run(0u, last_token - 1u, last_token);
  probe.set_run(1u, 1u, 1u);
  CHECK(!retired.invariants_hold()); }
