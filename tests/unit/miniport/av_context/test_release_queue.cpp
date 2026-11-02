#include "harness/framework/test_registry.hpp"
#include "miniport/av_context/release_queue.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <cstdint>

using av::ReleaseQueue;
using av::SubmissionDescriptor;

namespace {

SubmissionDescriptor batch(std::uint64_t fence, std::uint64_t begin, std::uint64_t end)
{ return SubmissionDescriptor(fence, begin, end); }

}

TEST(release_queue, batches_in_order_are_released_as_they_arrive)
{ ReleaseQueue queue;
  CHECK(!queue.active());
  CHECK(queue.arrive(batch(1u, 0u, 64u), 0u) == ReleaseQueue::Turn::release_now);
  queue.released(batch(1u, 0u, 64u));
  CHECK(queue.active());
  CHECK(queue.arrive(batch(2u, 64u, 128u), 0u) == ReleaseQueue::Turn::release_now); }

TEST(release_queue, a_batch_that_comes_first_waits_for_the_one_before_it)
{ ReleaseQueue queue;
  CHECK(queue.arrive(batch(2u, 64u, 128u), 0u) == ReleaseQueue::Turn::held);
  CHECK(queue.arrive(batch(3u, 128u, 192u), 0u) == ReleaseQueue::Turn::held);
  CHECK_EQ(queue.held(), 2u);
  CHECK(queue.arrive(batch(1u, 0u, 64u), 0u) == ReleaseQueue::Turn::release_now);
  queue.released(batch(1u, 0u, 64u));
  SubmissionDescriptor next;
  CHECK(queue.next_held(next));
  CHECK(next.fence_id() == 2u);
  queue.released(next);
  CHECK(queue.next_held(next));
  CHECK(next.fence_id() == 3u);
  queue.released(next);
  CHECK(!queue.next_held(next));
  CHECK_EQ(queue.held(), 0u); }

TEST(release_queue, the_order_begins_at_the_tail_so_an_earlier_unpublished_batch_holds_the_first_release)
{ ReleaseQueue queue;
  CHECK(queue.arrive(batch(2u, 64u, 128u), 0u) == ReleaseQueue::Turn::held);
  CHECK(queue.active()); }

TEST(release_queue, a_batch_seen_again_is_done_or_held_once)
{ ReleaseQueue queue;
  CHECK(queue.arrive(batch(1u, 0u, 64u), 0u) == ReleaseQueue::Turn::release_now);
  queue.released(batch(1u, 0u, 64u));
  CHECK(queue.arrive(batch(1u, 0u, 64u), 0u) == ReleaseQueue::Turn::released_already);
  CHECK(queue.arrive(batch(3u, 128u, 192u), 0u) == ReleaseQueue::Turn::held);
  CHECK(queue.arrive(batch(3u, 128u, 192u), 0u) == ReleaseQueue::Turn::held);
  CHECK_EQ(queue.held(), 1u); }

TEST(release_queue, an_overlap_and_a_full_queue_are_refused)
{ ReleaseQueue queue;
  CHECK(queue.arrive(batch(1u, 0u, 64u), 0u) == ReleaseQueue::Turn::release_now);
  queue.released(batch(1u, 0u, 64u));
  CHECK(queue.arrive(batch(2u, 32u, 96u), 0u) == ReleaseQueue::Turn::refused);
  for (std::uint32_t index = 0u; index < ReleaseQueue::most_held; index++)
      CHECK(queue.arrive(batch(10u + index, 1000u + 64u * index, 1064u + 64u * index), 0u) ==
            ReleaseQueue::Turn::held);
  CHECK(queue.arrive(batch(999u, 100000u, 100064u), 0u) == ReleaseQueue::Turn::refused); }

TEST(release_queue, clear_forgets_what_was_held_and_begins_an_order_again)
{ ReleaseQueue queue;
  CHECK(queue.arrive(batch(2u, 64u, 128u), 0u) == ReleaseQueue::Turn::held);
  queue.clear();
  CHECK(!queue.active());
  CHECK_EQ(queue.held(), 0u);
  CHECK(queue.arrive(batch(5u, 500u, 564u), 500u) == ReleaseQueue::Turn::release_now); }
