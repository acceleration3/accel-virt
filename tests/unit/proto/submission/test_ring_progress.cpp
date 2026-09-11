#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/ring_progress.hpp"

#include <cstdint>

using av::RingHeader;
using av::RingProgress;
using av::Status;

namespace av {

struct RingProgressProbe {
    explicit RingProgressProbe(RingProgress &target) : progress(target) {}
    void set_head(std::uint64_t head) { progress.head_ = head; }
    void set_order(std::uint8_t order) { progress.order_ = order; }
    RingProgress &progress;
};

}  // namespace av

using av::RingProgressProbe;

namespace {

constexpr std::uint64_t page = 4096u;
constexpr std::uint64_t top = ~std::uint64_t{0};

}  // namespace

TEST(ring_progress, starts_unobserved)
{ const RingProgress progress;
  CHECK(!progress.started());
  CHECK(progress.invariants_hold()); }

TEST(ring_progress_observe, the_first_consistent_snapshot_is_accepted)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u, 40u)), Status::ok);
  CHECK(progress.invariants_hold());
  CHECK(progress.started());
  CHECK_EQ(progress.order(), 12u);
  CHECK(progress.tail() == 100u);
  CHECK(progress.head() == 40u); }

TEST(ring_progress_observe, legal_progress_on_both_counters)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u, 40u)), Status::ok);
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u, 100u)), Status::ok);
  CHECK(progress.invariants_hold());
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u + page, 100u)), Status::ok);
  CHECK(progress.invariants_hold()); }

TEST(ring_progress_observe, head_moving_backwards_is_rejected_and_the_last_good_kept)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u, 60u)), Status::ok);
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u, 59u)), Status::invalid_state);
  CHECK(progress.head() == 60u);
  CHECK(progress.invariants_hold()); }

TEST(ring_progress_observe, tail_moving_backwards_is_rejected)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u, 60u)), Status::ok);
  CHECK_STATUS(progress.observe(RingHeader(12u, 99u, 60u)), Status::invalid_state);
  CHECK(progress.tail() == 100u); }

TEST(ring_progress_observe, head_past_tail_is_rejected)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, 10u, 11u)), Status::invalid_state);
  CHECK(!progress.started()); }

TEST(ring_progress_observe, more_than_the_capacity_outstanding_is_rejected)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, page, 0u)), Status::ok);
  CHECK_STATUS(progress.observe(RingHeader(12u, page + 1u, 0u)), Status::invalid_state);
  CHECK(progress.tail() == page); }

TEST(ring_progress_observe, a_changed_order_is_rejected)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, 0u, 0u)), Status::ok);
  CHECK_STATUS(progress.observe(RingHeader(13u, 0u, 0u)), Status::invalid_state);
  CHECK_EQ(progress.order(), 12u); }

TEST(ring_progress_observe, an_order_outside_the_protocol_range_is_rejected)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(11u, 0u, 0u)), Status::invalid_state);
  CHECK_STATUS(progress.observe(RingHeader(25u, 0u, 0u)), Status::invalid_state);
  CHECK_STATUS(progress.observe(RingHeader(64u, 0u, 0u)), Status::invalid_state);
  CHECK(!progress.started()); }

TEST(ring_progress_observe, counters_near_2_to_64_compare_without_overflow)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(24u, top, top - 1u)), Status::ok);
  CHECK_STATUS(progress.observe(RingHeader(24u, top, top)), Status::ok);
  CHECK_STATUS(progress.observe(RingHeader(24u, 0u, 0u)), Status::invalid_state);
  CHECK(progress.invariants_hold()); }

TEST(ring_progress_invariants, a_corrupted_tracker_is_detected)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, 100u, 50u)), Status::ok);
  RingProgressProbe probe(progress);
  probe.set_head(101u);
  CHECK(!progress.invariants_hold());
  probe.set_head(50u);
  probe.set_order(30u);
  CHECK(!progress.invariants_hold()); }

TEST(ring_progress_invariants, outstanding_bytes_above_capacity_are_detected)
{ RingProgress progress;
  CHECK_STATUS(progress.observe(RingHeader(12u, page, 0u)), Status::ok);
  RingProgressProbe(progress).set_order(11u);
  CHECK(!progress.invariants_hold()); }
