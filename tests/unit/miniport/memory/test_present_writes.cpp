#include "miniport/memory/present_writes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::Irql;
using av::MockPlatform;
using av::PresentWrites;

TEST(present_writes, nothing_has_been_written_before_the_first_present)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  CHECK_EQ(writes.sequence(), 0u);
  CHECK(!writes.in_progress(writes.sequence()));
  CHECK_EQ(writes.ticket(), 0u);
  CHECK_EQ(writes.epoch(), 0u);
  CHECK(writes.invariants_hold()); }

TEST(present_writes, a_present_is_in_progress_from_its_opening_until_it_is_written)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  writes.opening();
  CHECK_EQ(writes.sequence(), 1u);
  CHECK(writes.in_progress(writes.sequence()));
  CHECK(writes.invariants_hold());
  writes.written(5u, 1u);
  CHECK_EQ(writes.sequence(), 2u);
  CHECK(!writes.in_progress(writes.sequence()));
  CHECK_EQ(writes.ticket(), 5u);
  CHECK(writes.invariants_hold());
  writes.opening();
  CHECK(writes.in_progress(writes.sequence()));
  CHECK_EQ(writes.ticket(), 5u);
  writes.written(6u, 1u);
  CHECK_EQ(writes.sequence(), 4u);
  CHECK_EQ(writes.ticket(), 6u);
  CHECK(writes.invariants_hold()); }

TEST(present_writes, a_present_moved_in_place_counts_a_write_and_leaves_the_ticket)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  writes.opening();
  writes.written(0u, 0u);
  CHECK_EQ(writes.sequence(), 2u);
  CHECK_EQ(writes.ticket(), 0u);
  CHECK(writes.invariants_hold());
  writes.opening();
  writes.written(9u, 1u);
  writes.opening();
  writes.written(0u, 0u);
  CHECK_EQ(writes.sequence(), 6u);
  CHECK_EQ(writes.ticket(), 9u);
  CHECK(writes.invariants_hold()); }

TEST(present_writes, in_progress_is_the_odd_counts_and_only_those)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  CHECK(!writes.in_progress(0u));
  CHECK(writes.in_progress(1u));
  CHECK(!writes.in_progress(2u));
  CHECK(writes.in_progress(~std::uint64_t{0}));
  CHECK(!writes.in_progress(~std::uint64_t{0} - 1u)); }

TEST(present_writes, is_legal_at_dispatch_for_a_present_the_dpc_and_a_reader_alike)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  platform.set_irql(Irql::dispatch);
  writes.opening();
  CHECK(writes.in_progress(writes.sequence()));
  writes.written(3u, 1u);
  CHECK_EQ(writes.ticket(), 3u);
  CHECK_EQ(writes.epoch(), 1u);
  CHECK(writes.invariants_hold());
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_writes, the_ticket_is_kept_with_the_epoch_of_the_engine_that_numbered_it)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  writes.opening();
  writes.written(40u, 1u);
  CHECK_EQ(writes.ticket(), 40u);
  CHECK_EQ(writes.epoch(), 1u);
  writes.opening();
  writes.written(0u, 7u);
  CHECK_EQ(writes.epoch(), 1u);
  CHECK_EQ(writes.ticket(), 40u);
  writes.opening();
  writes.written(2u, 2u);
  CHECK_EQ(writes.ticket(), 2u);
  CHECK_EQ(writes.epoch(), 2u);
  CHECK(writes.invariants_hold()); }

TEST(present_writes, two_presents_open_at_once_are_both_counted_and_both_must_be_written)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  writes.opening();
  writes.opening();
  CHECK_EQ(writes.sequence(), 2u);
  CHECK(writes.in_progress(writes.sequence()));
  writes.written(0u, 0u);
  CHECK_EQ(writes.sequence(), 3u);
  CHECK(writes.in_progress(writes.sequence()));
  CHECK(writes.invariants_hold());
  writes.written(0u, 0u);
  CHECK_EQ(writes.sequence(), 4u);
  CHECK(!writes.in_progress(writes.sequence()));
  CHECK(writes.in_progress(writes.sequence() + 1u));
  CHECK(writes.invariants_hold()); }

TEST(present_writes_invariants, a_write_closed_that_was_never_opened_is_detected)
{ MockPlatform  platform;
  PresentWrites writes(platform);
  writes.written(0u, 0u);
  CHECK(!writes.invariants_hold()); }
