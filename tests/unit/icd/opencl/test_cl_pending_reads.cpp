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
using av::clicd::ClPendingReads;

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
