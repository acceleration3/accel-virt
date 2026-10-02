/* The fences a ring ran whose GPU work has not finished: released oldest first
 * once the GPU passes each one's mark, and never past one still held, because
 * the guest reads a context's completions as one rising value. */
#include "device/context/held_completions.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HeldCompletions;

namespace av {

struct HeldCompletionsProbe {
    static void unorder(HeldCompletions &held) { held.fences_[(held.head_ + 1u) % HeldCompletions::capacity] = 0u; }
    static void overcount(HeldCompletions &held) { held.count_ = HeldCompletions::capacity + 1u; }
    static void move_head_out(HeldCompletions &held) { held.head_ = HeldCompletions::capacity; }
};

}  // namespace av

TEST(held_completions, starts_empty_and_releases_nothing)
{ HeldCompletions held;
  CHECK(held.empty());
  CHECK(held.release(~std::uint64_t{0}) == 0u);
  CHECK(held.release_all() == 0u);
  CHECK(held.invariants_hold()); }

TEST(held_completions_release, answers_the_newest_fence_whose_mark_was_reached)
{ HeldCompletions held;
  CHECK(held.hold(1u, 10u));
  CHECK(held.hold(2u, 20u));
  CHECK(held.hold(3u, 30u));
  CHECK(held.invariants_hold());
  CHECK(held.release(9u) == 0u);    /* the oldest mark not reached: nothing goes */
  CHECK(held.release(10u) == 1u);   /* reached exactly is reached */
  CHECK(held.release(29u) == 2u);
  CHECK(!held.empty());
  CHECK(held.release(30u) == 3u);
  CHECK(held.empty());
  CHECK(held.invariants_hold()); }

TEST(held_completions_release, a_fence_behind_one_still_held_stays_held_whatever_its_mark)
{ HeldCompletions held;
  CHECK(held.hold(4u, 50u));
  CHECK(held.hold(5u, 5u));
  CHECK(held.release(40u) == 0u);
  CHECK(!held.empty());
  CHECK(held.release(50u) == 5u);
  CHECK(held.empty()); }

TEST(held_completions_release_all, lets_every_fence_go_and_answers_the_newest)
{ HeldCompletions held;
  CHECK(held.hold(7u, ~std::uint64_t{0}));
  CHECK(held.hold(8u, 3u));
  CHECK(held.release_all() == 8u);
  CHECK(held.empty());
  CHECK(held.release_all() == 0u); }

TEST(held_completions_hold, refuses_past_its_capacity_and_wraps_once_released)
{ HeldCompletions held;
  for (std::uint64_t fence = 1u; fence <= HeldCompletions::capacity; fence++) CHECK(held.hold(fence, fence));
  CHECK(!held.hold(HeldCompletions::capacity + 1u, 1u));
  CHECK(held.release(2u) == 2u);
  CHECK(held.hold(HeldCompletions::capacity + 1u, HeldCompletions::capacity + 1u));
  CHECK(held.hold(HeldCompletions::capacity + 2u, HeldCompletions::capacity + 2u));
  CHECK(!held.hold(HeldCompletions::capacity + 3u, 1u));
  CHECK(held.invariants_hold());
  CHECK(held.release(HeldCompletions::capacity + 2u) == HeldCompletions::capacity + 2u);
  CHECK(held.empty());
  CHECK(held.invariants_hold()); }

TEST(held_completions_invariants, see_fences_out_of_order_a_count_past_capacity_and_a_head_outside)
{ HeldCompletions unordered;
  CHECK(unordered.hold(3u, 1u));
  CHECK(unordered.hold(4u, 1u));
  av::HeldCompletionsProbe::unorder(unordered);
  CHECK(!unordered.invariants_hold());
  HeldCompletions overfull;
  av::HeldCompletionsProbe::overcount(overfull);
  CHECK(!overfull.invariants_hold());
  HeldCompletions outside;
  av::HeldCompletionsProbe::move_head_out(outside);
  CHECK(!outside.invariants_hold()); }
