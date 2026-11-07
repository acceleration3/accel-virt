#include "executor/d3d/idle_queue.hpp"
#include "executor/d3d/queue_drain.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::QueueDrain;

namespace {

class FakeQueue final : public av::IdleQueue {
public:
    bool mark_idle(std::uint64_t &ticket) override
    {
        marks++;
        ticket = 0u;
        if (refuse) return false;
        ticket = ++next;
        return true;
    }
    bool reached(std::uint64_t ticket, std::uint64_t) const override
    {
        asked = ticket;
        return ticket <= done;
    }

    std::uint32_t         marks = 0u;
    std::uint64_t         next = 0u;
    std::uint64_t         done = 0u;
    bool                  refuse = false;
    mutable std::uint64_t asked = 0u;
};

}

TEST(queue_drain, a_mark_not_yet_reached_is_polled_again_and_no_new_mark_is_queued_without_new_work)
{ FakeQueue queue;
  QueueDrain drain;
  CHECK(drain.mark(queue, 5u));
  CHECK(!drain.reached(queue, 0u));
  CHECK(drain.mark(queue, 5u));
  CHECK_EQ(queue.marks, 1u);
  queue.done = 1u;
  CHECK(drain.reached(queue, 0u));
  CHECK_EQ(queue.asked, 1u); }

TEST(queue_drain, new_work_queues_a_new_mark_and_the_old_one_no_longer_counts)
{ FakeQueue queue;
  QueueDrain drain;
  CHECK(drain.mark(queue, 5u));
  CHECK(drain.mark(queue, 6u));
  CHECK_EQ(queue.marks, 2u);
  queue.done = 1u;
  CHECK(!drain.reached(queue, 0u));
  CHECK_EQ(queue.asked, 2u); }

TEST(queue_drain, a_refused_mark_is_retried_and_nothing_is_reached_before_one_is_made)
{ FakeQueue queue;
  QueueDrain drain;
  queue.refuse = true;
  CHECK(!drain.mark(queue, 1u));
  CHECK(!drain.marked());
  CHECK(!drain.reached(queue, 0u));
  queue.refuse = false;
  CHECK(drain.mark(queue, 1u));
  CHECK_EQ(queue.marks, 2u); }

TEST(queue_drain, forgetting_drops_the_mark)
{ FakeQueue queue;
  QueueDrain drain;
  CHECK(drain.mark(queue, 1u));
  drain.forget();
  CHECK(!drain.marked());
  CHECK(drain.mark(queue, 1u));
  CHECK_EQ(queue.marks, 2u); }
