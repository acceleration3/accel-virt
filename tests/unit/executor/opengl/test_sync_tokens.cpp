#include "executor/opengl/sync_tokens.hpp"
#include "harness/executor/retired_sync_tokens_probe.hpp"
#include "harness/executor/sync_tokens_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::MockPlatform;
using av::RetiredSyncTokensProbe;
using av::Status;
using av::SyncTokens;
using av::SyncTokensProbe;

namespace {

int first = 0;
int second = 0;

int syncs[SyncTokens::retire_after + 1u];

void fill(SyncTokens &tokens)
{
    for (std::uint32_t i = 0u; i < SyncTokens::retire_after; i++) CHECK(tokens.bind(i + 1u, &syncs[i]) == Status::ok);
    CHECK(tokens.crowded());
}

void *retire(SyncTokens &tokens)
{
    void *deleted = &first;
    CHECK(tokens.retire_oldest(deleted) == Status::ok);
    return deleted;
}

}

TEST(sync_tokens, a_bound_token_names_the_fence_it_was_bound_to)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.fence_for(7u) == nullptr);
  CHECK(tokens.bind(7u, &first) == Status::ok);
  CHECK(tokens.fence_for(7u) == &first);
  CHECK_EQ(tokens.count(), 1u);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, a_token_nobody_bound_names_nothing)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(7u, &first) == Status::ok);
  CHECK(tokens.fence_for(8u) == nullptr);
  CHECK(tokens.fence_for(6u) == nullptr);
  CHECK(tokens.fence_for(0u) == nullptr);
  CHECK(tokens.fence_for(~std::uint64_t{0}) == nullptr); }

TEST(sync_tokens, a_token_not_newer_than_the_newest_bound_is_refused)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(7u, &first) == Status::ok);
  CHECK(tokens.bind(7u, &second) == Status::invalid_parameter);
  CHECK(tokens.bind(6u, &second) == Status::invalid_parameter);
  CHECK(tokens.fence_for(7u) == &first);
  CHECK(tokens.fence_for(6u) == nullptr);
  CHECK(tokens.forget(7u) == &first);
  CHECK(tokens.bind(7u, &second) == Status::invalid_parameter);
  CHECK(tokens.bind(8u, &second) == Status::ok);
  CHECK_EQ(tokens.count(), 1u);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, a_zero_token_or_a_failed_fence_is_not_a_binding)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(0u, &first) == Status::invalid_parameter);
  CHECK(tokens.bind(7u, nullptr) == Status::invalid_parameter);
  CHECK_EQ(tokens.count(), 0u);
  CHECK(tokens.bind(7u, &first) == Status::ok);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, forgetting_one_answers_its_fence_and_leaves_the_rest)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.bind(2u, &second) == Status::ok);
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

TEST(sync_tokens, a_table_grows_past_crowded_and_refuses_no_fence)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(!tokens.crowded());
  CHECK(tokens.oldest() == nullptr);
  constexpr std::uint64_t fences = 10000u;
  static char made[fences];
  for (std::uint64_t token = 1u; token <= fences; token++) CHECK(tokens.bind(token, &made[token - 1u]) == Status::ok);
  CHECK(tokens.crowded());
  CHECK_EQ(tokens.count(), fences);
  CHECK(tokens.oldest() == &made[0]);
  bool answered = true;
  for (std::uint64_t token = 1u; token <= fences; token++) answered = answered && tokens.fence_for(token) == &made[token - 1u];
  CHECK(answered);
  CHECK(tokens.forget(fences / 2u) == &made[fences / 2u - 1u]);
  CHECK(tokens.fence_for(fences / 2u) == nullptr);
  CHECK(tokens.fence_for(fences / 2u + 1u) == &made[fences / 2u]);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, a_table_that_cannot_grow_refuses_the_fence)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  platform.fail_allocation_at(1u);
  CHECK(tokens.bind(1u, &first) == Status::no_memory);
  CHECK(tokens.fence_for(1u) == nullptr);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, a_retirement_with_no_room_for_its_token_keeps_the_sync)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  platform.fail_allocation_at(1u);
  void *deleted = &second;
  CHECK(tokens.retire_oldest(deleted) == Status::no_memory);
  CHECK(deleted == nullptr);
  CHECK_EQ(tokens.count(), 1u);
  CHECK(tokens.fence_for(1u) == &first);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, retiring_keeps_the_first_as_the_stand_in_and_hands_back_the_rest)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  fill(tokens);
  CHECK(retire(tokens) == nullptr);
  CHECK(tokens.invariants_hold());
  CHECK_EQ(tokens.count(), SyncTokens::retire_after - 1u);
  CHECK(tokens.oldest() == &syncs[1]);
  CHECK(tokens.fence_for(1u) == &syncs[0]);
  CHECK(retire(tokens) == &syncs[1]);
  CHECK(tokens.invariants_hold());
  CHECK(tokens.fence_for(2u) == &syncs[0]);
  CHECK(tokens.fence_for(1u) == &syncs[0]);
  CHECK(tokens.fence_for(3u) == &syncs[2]);
  CHECK(tokens.bind(2u, &first) == Status::invalid_parameter);
  CHECK(tokens.bind(SyncTokens::retire_after + 1u, &first) == Status::ok);
  CHECK(tokens.fence_for(SyncTokens::retire_after + 1u) == &first); }

TEST(sync_tokens, retiring_from_an_empty_table_does_nothing)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(retire(tokens) == nullptr);
  CHECK_EQ(tokens.count(), 0u);
  CHECK(tokens.fence_for(0u) == nullptr);
  CHECK(tokens.invariants_hold());
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(retire(tokens) == nullptr);
  CHECK(retire(tokens) == nullptr);
  CHECK(tokens.fence_for(1u) == &first);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, forgetting_a_retired_token_hands_back_nothing)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.bind(2u, &second) == Status::ok);
  CHECK(tokens.bind(3u, &syncs[0]) == Status::ok);
  CHECK(retire(tokens) == nullptr);
  CHECK(retire(tokens) == &second);
  CHECK(tokens.forget(1u) == nullptr);
  CHECK(tokens.invariants_hold());
  CHECK(tokens.fence_for(1u) == nullptr);
  CHECK(tokens.fence_for(2u) == &first);
  CHECK(tokens.forget(2u) == nullptr);
  CHECK(tokens.fence_for(2u) == nullptr);
  CHECK(tokens.invariants_hold());
  void *fence = nullptr;
  CHECK(tokens.take_any(fence));
  CHECK(fence == &syncs[0]);
  CHECK(tokens.take_any(fence));
  CHECK(fence == &first); }

TEST(sync_tokens, take_any_gives_every_fence_back_once_and_the_stand_in_last)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &syncs[0]) == Status::ok);
  CHECK(tokens.bind(2u, &first) == Status::ok);
  CHECK(tokens.bind(3u, &second) == Status::ok);
  CHECK(retire(tokens) == nullptr);
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
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.fence_for(1u) == &first); }

TEST(sync_tokens, take_any_of_an_empty_table_takes_nothing)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  void *fence = &first;
  CHECK(!tokens.take_any(fence));
  CHECK(fence == nullptr);
  CHECK(tokens.invariants_hold()); }

TEST(sync_tokens, a_guest_that_never_deletes_is_served_without_growing)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  constexpr std::uint64_t fences = 5000u;
  static char   made[fences];
  std::uint32_t handed_back = 0u;
  for (std::uint64_t token = 1u; token <= fences; token++) {
      if (tokens.crowded()) {
          void *const retired = retire(tokens);
          if (retired != nullptr) CHECK(retired == &made[token - SyncTokens::retire_after - 1u]);
          handed_back += retired != nullptr ? 1u : 0u;
      }
      CHECK(tokens.bind(token, &made[token - 1u]) == Status::ok);
      CHECK(tokens.invariants_hold());
      CHECK(tokens.count() <= SyncTokens::retire_after);
  }
  const std::uint64_t retired = fences - SyncTokens::retire_after;
  for (std::uint64_t token = 1u; token <= retired; token++) CHECK(tokens.fence_for(token) == &made[0]);
  for (std::uint64_t token = retired + 1u; token <= fences; token++)
      CHECK(tokens.fence_for(token) == &made[token - 1u]);
  CHECK(tokens.fence_for(fences + 1u) == nullptr);
  CHECK_EQ(tokens.count(), SyncTokens::retire_after);
  CHECK_EQ(handed_back, fences - SyncTokens::retire_after - 1u);
  CHECK_EQ(SyncTokensProbe(tokens).retired().runs(), 1u); }

TEST(sync_tokens_invariants, a_zero_token_in_the_table_breaks_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.bind(2u, &second) == Status::ok);
  SyncTokensProbe(tokens).set_token(0u, 0u);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, a_null_fence_in_the_table_breaks_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.bind(2u, &second) == Status::ok);
  SyncTokensProbe(tokens).set_fence(1u, nullptr);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, live_tokens_out_of_order_break_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.bind(2u, &second) == Status::ok);
  SyncTokensProbe(tokens).set_token(1u, 1u);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, a_live_token_past_the_newest_breaks_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(5u, &first) == Status::ok);
  SyncTokensProbe(tokens).set_newest(4u);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, retired_tokens_with_no_stand_in_break_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(retire(tokens) == nullptr);
  CHECK(tokens.invariants_hold());
  SyncTokensProbe(tokens).set_stand_in(nullptr);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, a_live_slot_holding_the_stand_in_breaks_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.bind(2u, &second) == Status::ok);
  CHECK(retire(tokens) == nullptr);
  SyncTokensProbe(tokens).set_fence(0u, &first);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, a_live_token_not_newer_than_the_retired_breaks_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(tokens.bind(2u, &second) == Status::ok);
  CHECK(retire(tokens) == nullptr);
  SyncTokensProbe(tokens).set_token(0u, 1u);
  CHECK(!tokens.invariants_hold()); }

TEST(sync_tokens_invariants, broken_retired_tokens_break_them)
{ MockPlatform platform;
  SyncTokens   tokens(platform);
  CHECK(tokens.bind(1u, &first) == Status::ok);
  CHECK(retire(tokens) == nullptr);
  SyncTokensProbe probe(tokens);
  RetiredSyncTokensProbe(probe.retired()).set_run(0u, 0u, 1u);
  CHECK(!tokens.invariants_hold()); }
