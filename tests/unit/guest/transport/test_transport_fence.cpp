/* The fences a context has in flight. Each batch a producer sends gets the next
 * id; the device completes them in ring order, so which are still outstanding
 * is the range (completed, next). The transport asks whether a fence still
 * needs waiting on, how many are in flight, and - when the ring is full - the
 * oldest outstanding fence, the one whose completion frees room. At most 64 are
 * outstanding at once. */
#include "core/support/status.hpp"
#include "guest/transport/transport_fence.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::TransportFence;
using av::TransportFenceProbe;

namespace av {

struct TransportFenceProbe {
    static void break_next(TransportFence &f) { f.next_ = 0u; }
    static void break_completed(TransportFence &f) { f.completed_ = 5u; f.next_ = 3u; }
    static void break_outstanding(TransportFence &f) { f.next_ = 200u; f.completed_ = 0u; }
};

}  // namespace av

TEST(transport_fence, opens_fences_from_one_and_none_is_pending_at_first)
{ TransportFence fence;
  CHECK_EQ(fence.outstanding(), 0u);
  CHECK_EQ(fence.completed(), 0u);
  CHECK_EQ(fence.oldest(), 0u);
  CHECK(!fence.full());
  CHECK(fence.invariants_hold()); }

TEST(transport_fence, each_open_is_the_next_id_and_becomes_outstanding)
{ TransportFence fence;
  CHECK_EQ(fence.open(), 1u);
  CHECK_EQ(fence.open(), 2u);
  CHECK_EQ(fence.open(), 3u);
  CHECK_EQ(fence.outstanding(), 3u);
  CHECK(fence.pending(2u));
  CHECK(!fence.pending(99u));   /* a fence never opened is not pending */
  CHECK_EQ(fence.oldest(), 1u);
  CHECK(fence.invariants_hold()); }

/* Completing the device's highest fence retires it and every earlier one. */
TEST(transport_fence, closing_advances_completed_and_retires_earlier_fences)
{ TransportFence fence;
  static_cast<void>(fence.open());
  static_cast<void>(fence.open());
  static_cast<void>(fence.open());
  fence.close(2u);
  CHECK_EQ(fence.completed(), 2u);
  CHECK_EQ(fence.outstanding(), 1u);
  CHECK(!fence.pending(1u));
  CHECK(!fence.pending(2u));
  CHECK(fence.pending(3u));
  CHECK_EQ(fence.oldest(), 3u);
  CHECK(fence.invariants_hold()); }

/* A completion that does not advance - a stale one, or one for a fence not yet
 * opened - changes nothing. */
TEST(transport_fence, a_stale_or_future_completion_is_ignored)
{ TransportFence fence;
  static_cast<void>(fence.open());
  static_cast<void>(fence.open());
  fence.close(1u);
  fence.close(1u);              /* stale: not greater than completed */
  CHECK_EQ(fence.completed(), 1u);
  fence.close(9u);              /* future: past what was opened */
  CHECK_EQ(fence.completed(), 1u);
  CHECK_EQ(fence.outstanding(), 1u);
  CHECK(fence.invariants_hold()); }

/* Once every fence is retired, nothing is pending or oldest. */
TEST(transport_fence, all_retired_leaves_nothing_outstanding)
{ TransportFence fence;
  const std::uint64_t last = fence.open();
  static_cast<void>(fence.open());
  fence.close(last + 1u);
  CHECK_EQ(fence.outstanding(), 0u);
  CHECK_EQ(fence.oldest(), 0u);
  CHECK(!fence.pending(1u));
  CHECK(fence.invariants_hold()); }

/* The table holds at most 64 in flight; the 65th open is refused until one is
 * closed, and then it succeeds. */
TEST(transport_fence, refuses_a_sixty_fifth_outstanding_fence)
{ TransportFence fence;
  for (std::uint32_t i = 0u; i < TransportFence::max_outstanding; i++)
      CHECK(fence.open() != 0u);
  CHECK(fence.full());
  CHECK_EQ(fence.open(), 0u);
  fence.close(1u);
  CHECK(!fence.full());
  CHECK(fence.open() != 0u);
  CHECK(fence.invariants_hold()); }

/* The invariant catches states the class never builds. */
TEST(transport_fence, the_invariant_catches_impossible_states)
{ TransportFence a;
  TransportFenceProbe::break_next(a);
  CHECK(!a.invariants_hold());
  TransportFence b;
  TransportFenceProbe::break_completed(b);
  CHECK(!b.invariants_hold());
  TransportFence c;
  TransportFenceProbe::break_outstanding(c);
  CHECK(!c.invariants_hold()); }
