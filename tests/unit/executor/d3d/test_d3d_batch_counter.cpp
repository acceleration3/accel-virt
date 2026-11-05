#include "executor/d3d/d3d_batch_counter.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dBatchCounter;

namespace {

using Marks = D3dBatchCounter::Marks;

constexpr Marks none = {0u, 0u, 0u};

Marks marks(std::uint64_t d3d11, std::uint64_t d3d12 = 0u, std::uint64_t d3d9 = 0u) { return Marks{d3d11, d3d12, d3d9}; }

}

TEST(d3d_batch_marks, are_covered_when_every_family_has_reached_them)
{ CHECK(marks(0u, 0u, 0u).covered_by(none));
  CHECK(marks(3u, 4u, 5u).covered_by(marks(3u, 4u, 5u)));
  CHECK(marks(3u, 4u, 5u).covered_by(marks(9u, 9u, 9u)));
  CHECK(!marks(4u, 4u, 5u).covered_by(marks(3u, 9u, 9u)));
  CHECK(!marks(3u, 5u, 5u).covered_by(marks(9u, 4u, 9u)));
  CHECK(!marks(3u, 4u, 6u).covered_by(marks(9u, 9u, 5u)));
  CHECK(marks(~0ull, ~0ull, ~0ull).covered_by(marks(~0ull, ~0ull, ~0ull)));
  CHECK(!marks(~0ull, 0u, 0u).covered_by(marks(~0ull - 1u, ~0ull, ~0ull))); }

TEST(d3d_batch_marks, compare_equal_only_when_all_three_families_match)
{ CHECK(marks(1u, 2u, 3u) == marks(1u, 2u, 3u));
  CHECK(!(marks(1u, 2u, 3u) == marks(0u, 2u, 3u)));
  CHECK(!(marks(1u, 2u, 3u) == marks(1u, 0u, 3u)));
  CHECK(!(marks(1u, 2u, 3u) == marks(1u, 2u, 0u))); }

TEST(d3d_batch_counter, a_fresh_counter_has_marked_nothing_and_waits_for_nothing)
{ const D3dBatchCounter counter;
  CHECK_EQ(counter.marked(), 0u);
  CHECK_EQ(counter.reached(none), 0u);
  CHECK(!counter.pending(none));
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, every_end_is_a_mark_whether_or_not_it_issued_work)
{ D3dBatchCounter counter;
  counter.end(none, none);
  counter.end(none, none);
  counter.end(marks(1u), marks(1u));
  CHECK_EQ(counter.marked(), 3u);
  CHECK_EQ(counter.reached(marks(1u)), 3u);
  CHECK(!counter.pending(marks(1u)));
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, a_batch_whose_work_is_already_done_is_reached_at_once)
{ D3dBatchCounter counter;
  counter.end(marks(5u, 2u, 1u), marks(5u, 2u, 1u));
  CHECK_EQ(counter.reached(marks(5u, 2u, 1u)), 1u);
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, a_batch_with_work_in_flight_is_reached_only_when_its_marks_are_done)
{ D3dBatchCounter counter;
  counter.end(marks(4u), none);
  CHECK_EQ(counter.marked(), 1u);
  CHECK_EQ(counter.reached(none), 0u);
  CHECK(counter.pending(none));
  CHECK_EQ(counter.reached(marks(3u)), 0u);
  CHECK_EQ(counter.reached(marks(4u)), 1u);
  CHECK(!counter.pending(marks(4u)));
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, reached_holds_just_before_the_first_unfinished_batch)
{ D3dBatchCounter counter;
  counter.end(marks(1u), marks(1u));
  counter.end(marks(2u), marks(1u));
  counter.end(marks(2u), marks(1u));
  counter.end(marks(3u), marks(1u));
  CHECK_EQ(counter.marked(), 4u);
  CHECK_EQ(counter.reached(marks(1u)), 1u);
  CHECK_EQ(counter.reached(marks(2u)), 3u);
  CHECK_EQ(counter.reached(marks(3u)), 4u);
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, a_batch_that_issued_nothing_new_adds_no_record_but_does_not_hide_an_earlier_one)
{ D3dBatchCounter counter;
  counter.end(marks(7u), none);
  counter.end(marks(7u), none);
  counter.end(marks(7u), none);
  CHECK_EQ(counter.marked(), 3u);
  CHECK_EQ(counter.reached(none), 0u);
  CHECK_EQ(counter.reached(marks(7u)), 3u);
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, the_first_batch_of_an_idle_counter_with_unchanged_marks_adds_no_record)
{ D3dBatchCounter counter;
  counter.end(none, none);
  CHECK_EQ(counter.reached(none), 1u);
  CHECK(!counter.pending(none));
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, a_family_that_has_not_reached_its_mark_holds_the_batch_back_even_if_others_have)
{ D3dBatchCounter counter;
  counter.end(marks(2u, 3u, 4u), none);
  CHECK_EQ(counter.reached(marks(9u, 9u, 3u)), 0u);
  CHECK_EQ(counter.reached(marks(9u, 2u, 9u)), 0u);
  CHECK_EQ(counter.reached(marks(1u, 9u, 9u)), 0u);
  CHECK_EQ(counter.reached(marks(2u, 3u, 4u)), 1u);
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_end, a_done_mark_retires_the_records_it_covers_from_the_front)
{ D3dBatchCounter counter;
  counter.end(marks(1u), none);
  counter.end(marks(2u), none);
  counter.end(marks(3u), marks(2u));
  CHECK_EQ(counter.reached(marks(2u)), 2u);
  CHECK_EQ(counter.marked(), 3u);
  counter.end(marks(3u), marks(3u));
  CHECK_EQ(counter.reached(marks(3u)), 4u);
  CHECK(!counter.pending(marks(3u)));
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_retire, drops_the_covered_prefix_and_keeps_what_is_behind_an_unfinished_batch)
{ D3dBatchCounter counter;
  counter.end(marks(1u, 0u), none);
  counter.end(marks(1u, 1u), none);
  counter.end(marks(2u, 1u), none);
  counter.retire(marks(1u, 0u));
  CHECK_EQ(counter.reached(marks(1u, 0u)), 1u);
  counter.retire(marks(2u, 0u));
  CHECK_EQ(counter.reached(marks(2u, 0u)), 1u);
  counter.retire(marks(2u, 1u));
  CHECK_EQ(counter.reached(none), 3u);
  CHECK(counter.invariants_hold());
  counter.retire(marks(9u, 9u, 9u));
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_retire, retiring_with_nothing_pending_changes_nothing)
{ D3dBatchCounter counter;
  counter.retire(marks(100u));
  CHECK_EQ(counter.marked(), 0u);
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_full, a_run_of_pending_batches_up_to_the_capacity_is_each_waited_for)
{ D3dBatchCounter counter;
  for (std::uint64_t batch = 1u; batch <= D3dBatchCounter::pending_capacity; batch++) {
      counter.end(marks(batch), none);
      CHECK(counter.invariants_hold());
  }
  CHECK_EQ(counter.marked(), D3dBatchCounter::pending_capacity);
  for (std::uint64_t done = 0u; done <= D3dBatchCounter::pending_capacity; done++)
      CHECK_EQ(counter.reached(marks(done)), done);
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_full, one_past_the_capacity_folds_into_the_newest_record_and_waits_for_all_of_it)
{ D3dBatchCounter counter;
  for (std::uint64_t batch = 1u; batch <= D3dBatchCounter::pending_capacity; batch++) counter.end(marks(batch), none);
  counter.end(marks(D3dBatchCounter::pending_capacity + 5u), none);
  CHECK_EQ(counter.marked(), D3dBatchCounter::pending_capacity + 1u);
  CHECK(counter.invariants_hold());
  CHECK_EQ(counter.reached(marks(D3dBatchCounter::pending_capacity - 1u)), D3dBatchCounter::pending_capacity - 1u);
  CHECK_EQ(counter.reached(marks(D3dBatchCounter::pending_capacity)), D3dBatchCounter::pending_capacity - 1u);
  CHECK_EQ(counter.reached(marks(D3dBatchCounter::pending_capacity + 4u)), D3dBatchCounter::pending_capacity - 1u);
  CHECK_EQ(counter.reached(marks(D3dBatchCounter::pending_capacity + 5u)), D3dBatchCounter::pending_capacity + 1u);
  CHECK(!counter.pending(marks(D3dBatchCounter::pending_capacity + 5u))); }

TEST(d3d_batch_counter_full, many_folds_never_lose_a_batch_and_keep_the_invariants)
{ D3dBatchCounter counter;
  for (std::uint64_t batch = 1u; batch <= 500u; batch++) {
      counter.end(marks(batch, batch / 2u, batch / 3u), none);
      CHECK(counter.invariants_hold());
  }
  CHECK_EQ(counter.marked(), 500u);
  CHECK_EQ(counter.reached(marks(500u, 250u, 166u)), 500u);
  CHECK(counter.reached(marks(499u, 250u, 166u)) < 500u);
  CHECK(counter.pending(marks(499u, 250u, 166u))); }

TEST(d3d_batch_counter_full, retiring_frees_room_so_the_next_batch_is_recorded_not_folded)
{ D3dBatchCounter counter;
  for (std::uint64_t batch = 1u; batch <= D3dBatchCounter::pending_capacity; batch++) counter.end(marks(batch), none);
  counter.retire(marks(10u));
  counter.end(marks(1000u), marks(10u));
  CHECK_EQ(counter.reached(marks(D3dBatchCounter::pending_capacity)), D3dBatchCounter::pending_capacity);
  CHECK_EQ(counter.reached(marks(999u)), D3dBatchCounter::pending_capacity);
  CHECK_EQ(counter.reached(marks(1000u)), D3dBatchCounter::pending_capacity + 1u);
  CHECK(counter.invariants_hold()); }

TEST(d3d_batch_counter_ring, wraps_around_its_storage_many_times_without_losing_order)
{ D3dBatchCounter counter;
  std::uint64_t     done = 0u;
  for (std::uint64_t batch = 1u; batch <= 1000u; batch++) {
      counter.end(marks(batch), marks(done));
      if (batch % 5u == 0u) done = batch - 3u;
      CHECK(counter.invariants_hold());
      const std::uint64_t reached = counter.reached(marks(done));
      CHECK(reached <= batch);
      CHECK(reached + 1u >= done);
  }
  CHECK_EQ(counter.reached(marks(1000u)), 1000u); }

TEST(d3d_batch_counter_families, reached_waits_for_the_slowest_family_across_a_mixed_run)
{ D3dBatchCounter counter;
  counter.end(marks(1u, 0u, 0u), none);
  counter.end(marks(1u, 1u, 0u), none);
  counter.end(marks(1u, 1u, 1u), none);
  CHECK_EQ(counter.reached(marks(1u, 0u, 0u)), 1u);
  CHECK_EQ(counter.reached(marks(1u, 1u, 0u)), 2u);
  CHECK_EQ(counter.reached(marks(0u, 1u, 1u)), 0u);
  CHECK_EQ(counter.reached(marks(1u, 1u, 1u)), 3u);
  CHECK(counter.invariants_hold()); }
