#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/fence_progress.hpp"

#include <cstdint>

using av::FencePage;
using av::FenceProgress;
using av::Status;

namespace av {

struct FenceProgressProbe {
    explicit FenceProgressProbe(FenceProgress &target) : progress(target) {}
    void set_completed(std::uint64_t completed) { progress.completed_ = completed; }
    FenceProgress &progress;
};

}  // namespace av

using av::FenceProgressProbe;

namespace {

constexpr std::uint64_t top = ~std::uint64_t{0};

}  // namespace

TEST(fence_progress, starts_with_nothing_submitted_or_completed)
{ const FenceProgress progress;
  CHECK(progress.highest_submitted() == 0u);
  CHECK(progress.completed_fence() == 0u);
  CHECK(progress.progress() == 0u);
  CHECK(!progress.faulted());
  CHECK(progress.invariants_hold()); }

TEST(fence_progress_record_submission, requires_strictly_increasing_ids)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(1u), Status::ok);
  CHECK(progress.invariants_hold());
  CHECK_STATUS(progress.record_submission(5u), Status::ok);
  CHECK_STATUS(progress.record_submission(5u), Status::invalid_state);
  CHECK_STATUS(progress.record_submission(4u), Status::invalid_state);
  CHECK(progress.highest_submitted() == 5u);
  CHECK(progress.invariants_hold()); }

TEST(fence_progress_record_submission, fence_id_zero_is_rejected)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(0u), Status::invalid_state); }

TEST(fence_progress_observe, a_page_before_any_completion_is_accepted)
{ FenceProgress progress;
  CHECK_STATUS(progress.observe(FencePage(0u, 0u)), Status::ok);
  CHECK(progress.invariants_hold()); }

TEST(fence_progress_observe, completion_up_to_the_highest_submission)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(3u), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(2u, 10u)), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(3u, 11u)), Status::ok);
  CHECK(progress.completed_fence() == 3u);
  CHECK(progress.progress() == 11u);
  CHECK(progress.invariants_hold()); }

TEST(fence_progress_observe, completing_an_unsubmitted_fence_is_rejected)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(3u), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(4u, 1u)), Status::invalid_state);
  CHECK(progress.completed_fence() == 0u); }

TEST(fence_progress_observe, a_completed_fence_moving_backwards_is_rejected)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(9u), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(6u, 20u)), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(5u, 21u)), Status::invalid_state);
  CHECK(progress.completed_fence() == 6u);
  CHECK(progress.progress() == 20u); }

TEST(fence_progress_observe, progress_moving_backwards_is_rejected)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(9u), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(6u, 20u)), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(6u, 19u)), Status::invalid_state); }

/* The raises a device counts only move forward, as progress does: one going
 * back is a page that contradicts what the driver already woke a process for. */
TEST(fence_progress_observe, raises_move_forward_and_moving_backwards_is_rejected)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(9u), Status::ok);
  CHECK(progress.raised() == 0u);
  CHECK_STATUS(progress.observe(FencePage(6u, 20u).raising(3u)), Status::ok);
  CHECK(progress.raised() == 3u);
  CHECK_STATUS(progress.observe(FencePage(6u, 21u).raising(2u)), Status::invalid_state);
  CHECK(progress.raised() == 3u);
  /* A device that counts none leaves it at zero, which is no move at all. */
  FenceProgress older;
  CHECK_STATUS(older.observe(FencePage(0u, 5u)), Status::ok);
  CHECK(older.raised() == 0u); }

/* The pulse's beat only moves forward, and moves alone: a page whose beat went
 * back is one no device wrote, whatever else it says. */
TEST(fence_progress_observe, the_beat_moves_alone_and_moving_backwards_is_rejected)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(9u), Status::ok);
  CHECK(progress.beat() == 0u);
  CHECK_STATUS(progress.observe(FencePage(6u, 20u).beating(40u)), Status::ok);
  CHECK(progress.beat() == 40u);
  CHECK_STATUS(progress.observe(FencePage(6u, 20u).beating(41u)), Status::ok);
  CHECK(progress.beat() == 41u);
  CHECK(progress.progress() == 20u);
  CHECK_STATUS(progress.observe(FencePage(7u, 21u).beating(40u)), Status::invalid_state);
  CHECK(progress.beat() == 41u);
  CHECK(progress.completed_fence() == 6u); }

TEST(fence_progress_observe, a_fault_is_latched)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(2u), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(1u, 5u, 77u)), Status::ok);
  CHECK(progress.faulted());
  CHECK_STATUS(progress.observe(FencePage(1u, 5u, 77u)), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(2u, 6u)), Status::invalid_state);
  CHECK(progress.faulted());
  CHECK(progress.completed_fence() == 1u); }

TEST(fence_progress, values_near_2_to_64_compare_without_overflow)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(top), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(top, top)), Status::ok);
  CHECK_STATUS(progress.record_submission(top), Status::invalid_state);
  CHECK(progress.invariants_hold()); }

TEST(fence_progress_invariants, completion_beyond_submission_is_detected)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(2u), Status::ok);
  FenceProgressProbe(progress).set_completed(3u);
  CHECK(!progress.invariants_hold()); }

/* Feature 049: once a process submits through its own doorbell, the driver
 * sees no submissions at all, so "nothing unsubmitted completes" is a
 * judgement it can no longer make - and it must not fail a context for the
 * completions that follow. Everything else it judges still holds. */
TEST(fence_progress, what_user_mode_submitted_is_completed_without_the_driver_having_seen_it)
{ FenceProgress progress;
  /* While the driver submits, a completion it never issued is a device that
   * contradicts what was seen. */
  CHECK_STATUS(progress.observe(FencePage(4u, 1u)), Status::invalid_state);
  CHECK(progress.completed_fence() == 0u);

  progress.user_mode_submits();
  CHECK_STATUS(progress.observe(FencePage(4u, 1u)), Status::ok);
  CHECK(progress.completed_fence() == 4u);
  CHECK(progress.highest_submitted() == 0u);
  CHECK(progress.invariants_hold());
  /* What it still judges: nothing moves backwards, and a fault stays. */
  CHECK_STATUS(progress.observe(FencePage(3u, 2u)), Status::invalid_state);
  CHECK_STATUS(progress.observe(FencePage(9u, 5u, 12u)), Status::ok);
  CHECK(progress.faulted());
  CHECK_STATUS(progress.observe(FencePage(10u, 6u)), Status::invalid_state);

  /* Handed back, the driver starts from what the device has finished rather
   * than from what it remembers issuing, which is nothing. */
  FenceProgress second;
  second.user_mode_submits();
  CHECK_STATUS(second.observe(FencePage(7u, 3u)), Status::ok);
  second.driver_submits();
  CHECK(second.highest_submitted() == 7u);
  CHECK(second.invariants_hold());
  CHECK_STATUS(second.record_submission(7u), Status::invalid_state);
  CHECK_STATUS(second.record_submission(8u), Status::ok);
  CHECK_STATUS(second.observe(FencePage(9u, 4u)), Status::invalid_state);
  CHECK_STATUS(second.observe(FencePage(8u, 4u)), Status::ok); }

/* Feature 052: a host event completes on a thread the device did not give the
 * host, and the device reports it by moving progress alone
 * (FenceReporter::raise). If the guest failed its context over that, the whole
 * out-of-band completion channel would be illegal. */
TEST(fence_progress_observe, progress_advancing_with_nothing_completed_is_accepted)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(4u), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(4u, 1u)), Status::ok);
  for (std::uint64_t moved = 2u; moved <= 6u; moved++) {
      CHECK_STATUS(progress.observe(FencePage(4u, moved)), Status::ok);
      CHECK(progress.completed_fence() == 4u);
      CHECK(progress.progress() == moved);
      CHECK(!progress.faulted());
  }
  /* The same page read twice is not a counter going backwards. */
  CHECK_STATUS(progress.observe(FencePage(4u, 6u)), Status::ok);
  CHECK(progress.progress() == 6u);
  CHECK(progress.invariants_hold());
  /* And before anything has completed at all, which is the wake a context gets
   * for its first host event. */
  FenceProgress fresh;
  CHECK_STATUS(fresh.observe(FencePage(0u, 1u)), Status::ok);
  CHECK(fresh.completed_fence() == 0u);
  CHECK(fresh.progress() == 1u);
  CHECK(fresh.invariants_hold()); }

/* Everything else the guest judges still holds around that shape: the wake
 * buys the device no licence to claim a batch finished, to walk either counter
 * back, or to recover from a fault. */
TEST(fence_progress_observe, an_out_of_band_wake_excuses_nothing_else)
{ FenceProgress progress;
  CHECK_STATUS(progress.record_submission(2u), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(2u, 5u)), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(2u, 6u)), Status::ok);
  CHECK_STATUS(progress.observe(FencePage(3u, 7u)), Status::invalid_state);
  CHECK_STATUS(progress.observe(FencePage(1u, 7u)), Status::invalid_state);
  CHECK_STATUS(progress.observe(FencePage(2u, 5u)), Status::invalid_state);
  /* A page the guest refused changed nothing it remembers. */
  CHECK(progress.completed_fence() == 2u);
  CHECK(progress.progress() == 6u);
  CHECK(!progress.faulted());
  CHECK(progress.invariants_hold());
  CHECK_STATUS(progress.observe(FencePage(2u, 7u, 3u)), Status::ok);
  CHECK(progress.faulted());
  CHECK_STATUS(progress.observe(FencePage(2u, 8u)), Status::invalid_state);
  CHECK(progress.faulted());
  CHECK(progress.progress() == 7u); }
