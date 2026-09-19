/* The non-blocking reads a process has outstanding.
 *
 * A read the application did not ask to block on cannot fetch its bytes at the
 * call - they do not exist yet - so what is kept until the event completes is
 * this: where in the buffer, how many bytes, and where the application wants
 * them. The bound is the point: an application may queue reads faster than it
 * waits for them, and the table must refuse rather than grow. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_pending_reads.hpp"

#include <cstdint>

using av::clicd::ClPendingRead;
using av::clicd::ClPendingReadProbe;
using av::clicd::ClPendingReads;
using av::clicd::ClPendingReadsProbe;

namespace av::clicd {

/* The states neither class ever builds. A row that is held but names no
 * destination would deliver bytes to nowhere; a row that holds nothing while
 * still naming one would deliver them to memory the application has taken
 * back; and a count that disagrees with the rows is a slot leaked or handed
 * out twice. */
struct ClPendingReadProbe {
    /* A held row with one of the five things a delivery needs missing. Each
     * is checked on its own, because the first to fail hides the rest. */
    static void forget_destination(ClPendingRead &row) { row.into_ = nullptr; }
    static void forget_event(ClPendingRead &row) { row.event_ = 0u; }
    static void forget_queue(ClPendingRead &row) { row.queue_ = 0u; }
    static void forget_buffer(ClPendingRead &row) { row.buffer_ = 0u; }
    static void forget_length(ClPendingRead &row) { row.bytes_ = 0u; }

    /* And the mirror image: a row holding nothing that still names one of
     * them, which is a slot that would be handed out while something else
     * still believes it is live. */
    static void abandon(ClPendingRead &row) { row.give_up(); }
    static void stray_destination(ClPendingRead &row) { row.into_ = &row; }
    static void stray_event(ClPendingRead &row) { row.event_ = 1u; }
    static void stray_queue(ClPendingRead &row) { row.queue_ = 1u; }
    static void stray_buffer(ClPendingRead &row) { row.buffer_ = 1u; }
    static void stray_length(ClPendingRead &row) { row.bytes_ = 1u; }
    static void stray_marker(ClPendingRead &row) { row.ours_ = true; }
};

struct ClPendingReadsProbe {
    static void miscount(ClPendingReads &reads) { reads.held_ += 1u; }
    static ClPendingRead &row(ClPendingReads &reads, std::uint32_t slot)
    { return reads.rows_[slot]; }
};

}  // namespace av::clicd

namespace {

constexpr std::uint64_t event = 0x6900u;
constexpr std::uint64_t queue = 0x6500u;
constexpr std::uint64_t buffer = 0x6600u;

}  // namespace

TEST(cl_pending_reads, a_recorded_read_is_found_by_its_event_and_by_nothing_else)
{ ClPendingReads reads;
  int destination = 0;
  CHECK_EQ(reads.outstanding(), 0u);
  CHECK(reads.waiting_on(event) == nullptr);

  CHECK(reads.record(event, queue, buffer, 16u, 4u, &destination, false));
  CHECK_EQ(reads.outstanding(), 1u);
  ClPendingRead *const waiting = reads.waiting_on(event);
  CHECK(waiting != nullptr);
  CHECK_EQ(waiting->queue(), queue);
  CHECK_EQ(waiting->buffer(), buffer);
  CHECK_EQ(waiting->offset(), 16u);
  CHECK_EQ(waiting->bytes(), 4u);
  CHECK(waiting->into() == &destination);
  /* An event of nothing is never a row, whatever the table holds. */
  CHECK(reads.waiting_on(0u) == nullptr);
  CHECK(reads.waiting_on(event + 1u) == nullptr);
  CHECK(reads.invariants_hold()); }

TEST(cl_pending_reads, a_row_that_names_nothing_to_read_or_nowhere_to_put_it_is_not_kept)
{ ClPendingReads reads;
  int destination = 0;
  CHECK(!reads.record(0u, queue, buffer, 0u, 4u, &destination, false));
  CHECK(!reads.record(event, 0u, buffer, 0u, 4u, &destination, false));
  CHECK(!reads.record(event, queue, 0u, 0u, 4u, &destination, false));
  CHECK(!reads.record(event, queue, buffer, 0u, 0u, &destination, false));
  CHECK(!reads.record(event, queue, buffer, 0u, 4u, nullptr, false));
  CHECK_EQ(reads.outstanding(), 0u);
  CHECK(reads.invariants_hold()); }

TEST(cl_pending_reads, a_delivered_read_gives_its_slot_back_and_is_counted)
{ ClPendingReads reads;
  int destination = 0;
  CHECK(reads.record(event, queue, buffer, 0u, 4u, &destination, false));
  CHECK_EQ(reads.delivered(), 0u);
  ClPendingRead *const waiting = reads.waiting_on(event);
  CHECK(waiting != nullptr);
  waiting->give_up();
  reads.count_delivery();
  CHECK_EQ(reads.outstanding(), 0u);
  CHECK_EQ(reads.delivered(), 1u);
  CHECK(reads.waiting_on(event) == nullptr);
  /* And the slot is reusable, which is what makes a long run of reads
   * possible at all. */
  CHECK(reads.record(event, queue, buffer, 0u, 4u, &destination, false));
  CHECK_EQ(reads.outstanding(), 1u);
  CHECK(reads.invariants_hold()); }

TEST(cl_pending_reads, a_table_that_is_full_refuses_rather_than_forgetting_one)
{ ClPendingReads reads;
  int destination = 0;
  for (std::uint32_t index = 0u; index < ClPendingReads::max_reads; ++index)
      CHECK(reads.record(event + index, queue, buffer, 0u, 4u, &destination, false));
  CHECK_EQ(reads.outstanding(), ClPendingReads::max_reads);
  /* One more is refused: the caller makes that read blocking instead, which
   * is slower and never wrong. Dropping a row would lose bytes an
   * application was told it would get. */
  CHECK(!reads.record(event + ClPendingReads::max_reads, queue, buffer, 0u, 4u, &destination,
                     false));
  CHECK_EQ(reads.outstanding(), ClPendingReads::max_reads);
  CHECK(reads.waiting_on(event) != nullptr);
  CHECK(reads.invariants_hold()); }

TEST(cl_pending_reads, releasing_a_queue_forgets_its_reads_and_leaves_every_other_queues)
{ ClPendingReads reads;
  int  destination = 0;
  const std::uint64_t other = queue + 1u;
  CHECK(reads.record(event, queue, buffer, 0u, 4u, &destination, false));
  CHECK(reads.record(event + 1u, other, buffer, 0u, 4u, &destination, true));
  CHECK(reads.record(event + 2u, queue, buffer, 8u, 4u, &destination, true));

  std::uint64_t markers[ClPendingReads::max_reads] = {};
  /* One of the two was the ICD's own marker, and it comes back so the caller
   * can give it to the host; the other was the application's event. */
  CHECK_EQ(reads.forget_queue(queue, markers), 1u);
  CHECK_EQ(markers[0], event + 2u);
  CHECK_EQ(reads.outstanding(), 1u);
  CHECK(reads.waiting_on(event) == nullptr);
  CHECK(reads.waiting_on(event + 2u) == nullptr);
  CHECK(reads.waiting_on(event + 1u) != nullptr);
  /* Forgotten, not delivered: nobody will ever wait for them. */
  CHECK_EQ(reads.delivered(), 0u);
  /* A caller that wants no markers back still has its rows forgotten: the
   * events leak on the host rather than the guest keeping rows for a queue
   * that is gone. */
  CHECK(reads.record(event + 3u, other, buffer, 0u, 4u, &destination, true));
  CHECK_EQ(reads.forget_queue(other, nullptr), 0u);
  CHECK_EQ(reads.outstanding(), 0u);
  CHECK(reads.invariants_hold());
  CHECK(reads.record(event, queue, buffer, 0u, 4u, &destination, false));

  /* A queue nothing was recorded on, and no queue at all, change nothing. */
  CHECK_EQ(reads.forget_queue(queue + 99u, markers), 0u);
  CHECK_EQ(reads.forget_queue(0u, markers), 0u);
  CHECK_EQ(reads.outstanding(), 1u);
  CHECK(reads.invariants_hold()); }

TEST(cl_pending_reads, a_slot_past_the_table_answers_a_row_that_holds_nothing)
{ ClPendingReads reads;
  /* The walk a finish does asks by slot, so a slot past the end has to
   * answer something rather than read the array. */
  CHECK(!reads.at(ClPendingReads::max_reads)->held());
  CHECK(!reads.at(0xFFFFFFFFu)->held());
  CHECK(reads.invariants_hold()); }

/* A held row is one a delivery can act on: every one of the five things it
 * needs is there. Missing any single one is a row that would read from
 * nowhere, or write to nowhere, and the invariant says so. */
TEST(cl_pending_reads, the_invariant_catches_a_held_row_that_could_not_deliver)
{ int destination = 0;
  const auto broken = [&destination](void (*corrupt)(ClPendingRead &)) {
      ClPendingReads reads;
      CHECK(reads.record(event, queue, buffer, 0u, 4u, &destination, false));
      CHECK(reads.invariants_hold());
      corrupt(ClPendingReadsProbe::row(reads, 0u));
      return reads.invariants_hold();
  };
  CHECK(!broken(ClPendingReadProbe::forget_destination));
  CHECK(!broken(ClPendingReadProbe::forget_event));
  CHECK(!broken(ClPendingReadProbe::forget_queue));
  CHECK(!broken(ClPendingReadProbe::forget_buffer));
  CHECK(!broken(ClPendingReadProbe::forget_length)); }

/* And the mirror: a row holding nothing must name nothing, or the slot gets
 * handed out while something still believes it is live. */
TEST(cl_pending_reads, the_invariant_catches_a_row_that_holds_nothing_but_still_names_something)
{ int destination = 0;
  const auto broken = [&destination](void (*corrupt)(ClPendingRead &)) {
      ClPendingReads reads;
      CHECK(reads.record(event, queue, buffer, 0u, 4u, &destination, false));
      ClPendingRead &row = ClPendingReadsProbe::row(reads, 0u);
      ClPendingReadProbe::abandon(row);
      reads.count_delivery();
      CHECK(reads.invariants_hold());
      corrupt(row);
      return reads.invariants_hold();
  };
  CHECK(!broken(ClPendingReadProbe::stray_destination));
  CHECK(!broken(ClPendingReadProbe::stray_event));
  CHECK(!broken(ClPendingReadProbe::stray_queue));
  CHECK(!broken(ClPendingReadProbe::stray_buffer));
  CHECK(!broken(ClPendingReadProbe::stray_length));
  CHECK(!broken(ClPendingReadProbe::stray_marker)); }

TEST(cl_pending_reads, the_invariant_catches_a_count_that_disagrees_with_the_rows)
{ ClPendingReads reads;
  int destination = 0;
  CHECK(reads.record(event, queue, buffer, 0u, 4u, &destination, false));
  CHECK(reads.invariants_hold());
  /* One more counted than held is a slot that can never be handed out. */
  ClPendingReadsProbe::miscount(reads);
  CHECK(!reads.invariants_hold());

  /* And a count past the table at all. */
  ClPendingReads beyond;
  for (std::uint32_t index = 0u; index < ClPendingReads::max_reads; ++index)
      CHECK(beyond.record(event + index, queue, buffer, 0u, 4u, &destination, false));
  CHECK(beyond.invariants_hold());
  ClPendingReadsProbe::miscount(beyond);
  CHECK(!beyond.invariants_hold()); }
