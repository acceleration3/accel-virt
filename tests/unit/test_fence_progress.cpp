#include "../../src/core/status.hpp"
#include "../../src/proto/fence_page.hpp"
#include "../../src/proto/fence_progress.hpp"
#include "../harness/test_registry.hpp"

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
