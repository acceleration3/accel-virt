#include "miniport/memory/present_reads.hpp"
#include "core/platform/lock_rank.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "miniport/memory/present_writes.hpp"

#include <cstdint>

namespace av {

struct PresentReadsProbe {
    static void open_without_expiry(PresentReads &reads) { reads.open_ = 1u; reads.expires_ = 0u; }
};

}

using av::Irql;
using av::MockPlatform;
using av::PresentReads;
using av::PresentReadsProbe;
using av::PresentWrites;
using av::Violation;

TEST(present_reads, admits_a_present_and_opens_it_while_no_read_is_open)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  CHECK(reads.admit(writes));
  CHECK_EQ(writes.sequence(), 1u);
  CHECK(writes.in_progress(writes.sequence()));
  CHECK(reads.admit(writes));
  CHECK_EQ(writes.sequence(), 2u);
  CHECK(reads.invariants_hold());
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_reads, an_open_read_refuses_a_present_and_leaves_the_writes_as_they_were)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  CHECK(!reads.open());
  CHECK(!reads.admit(writes));
  CHECK_EQ(writes.sequence(), 0u);
  CHECK(!writes.in_progress(writes.sequence()));
  CHECK(reads.invariants_hold()); }

TEST(present_reads, closing_the_read_admits_the_presents_again)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  CHECK(!reads.open());
  reads.close();
  CHECK(reads.admit(writes));
  CHECK_EQ(writes.sequence(), 1u);
  CHECK(reads.invariants_hold()); }

TEST(present_reads, a_present_is_admitted_only_when_every_open_read_has_closed)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  CHECK(!reads.open());
  CHECK(!reads.open());
  reads.close();
  CHECK(!reads.admit(writes));
  reads.close();
  CHECK(reads.admit(writes)); }

TEST(present_reads, a_close_with_no_read_open_is_no_error_and_opens_nothing)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  reads.close();
  reads.close();
  CHECK(reads.admit(writes));
  CHECK(!reads.open());
  CHECK(!reads.admit(writes));
  CHECK(reads.invariants_hold()); }

TEST(present_reads, a_read_expires_after_the_bound_and_stops_holding_presents)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  CHECK(!reads.open());
  platform.advance_time(PresentReads::expiry_ticks - 1u);
  CHECK(!reads.admit(writes));
  platform.advance_time(1u);
  CHECK(reads.admit(writes));
  CHECK_EQ(writes.sequence(), 1u); }

TEST(present_reads, the_next_begin_forgets_a_read_that_expired_and_says_so)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  CHECK(!reads.open());
  platform.advance_time(PresentReads::expiry_ticks);
  CHECK(reads.open());
  CHECK(!reads.admit(writes));
  reads.close();
  CHECK(reads.admit(writes));
  CHECK(reads.invariants_hold()); }

TEST(present_reads, a_second_read_extends_the_hold_from_its_own_begin)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  CHECK(!reads.open());
  platform.advance_time(PresentReads::expiry_ticks / 2u);
  CHECK(!reads.open());
  platform.advance_time(PresentReads::expiry_ticks / 2u + 1u);
  CHECK(!reads.admit(writes));
  platform.advance_time(PresentReads::expiry_ticks / 2u);
  CHECK(reads.admit(writes)); }

TEST(present_reads, a_read_opened_near_the_end_of_time_is_held_and_its_expiry_not_wrapped)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  platform.advance_time(~std::uint64_t{0} - 10u);
  CHECK(!reads.open());
  CHECK(!reads.admit(writes));
  platform.advance_time(5u);
  CHECK(!reads.admit(writes));
  CHECK_EQ(writes.sequence(), 0u); }

TEST(present_reads, is_legal_up_to_dispatch_and_not_above)
{ MockPlatform platform;
  PresentReads reads(platform);
  PresentWrites writes(platform);
  platform.set_irql(Irql::dispatch);
  CHECK(!reads.open());
  CHECK(!reads.admit(writes));
  reads.close();
  CHECK(reads.admit(writes));
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  platform.set_irql(Irql::isr);
  static_cast<void>(reads.open());
  CHECK(platform.claim(Violation::irql_ceiling));
  static_cast<void>(reads.admit(writes));
  CHECK(platform.claim(Violation::irql_ceiling));
  reads.close();
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive);
  while (platform.claim(Violation::irql_ceiling)) {}
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_reads_invariants, a_read_open_without_an_expiry_is_detected)
{ MockPlatform platform;
  PresentReads reads(platform);
  CHECK(reads.invariants_hold());
  PresentReadsProbe::open_without_expiry(reads);
  CHECK(!reads.invariants_hold()); }
