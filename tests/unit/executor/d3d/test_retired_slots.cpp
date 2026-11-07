#include "executor/d3d/idle_queue.hpp"
#include "executor/d3d/retired_slots.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::RetiredSlots;

namespace {

class FakeQueue final : public av::IdleQueue {
public:
    bool mark_idle(std::uint64_t &ticket) override
    {
        ticket = 0u;
        if (refuse) return false;
        ticket = ++next;
        return true;
    }
    bool reached(std::uint64_t ticket, std::uint64_t) const override { return ticket <= done; }

    std::uint64_t next = 0u;
    std::uint64_t done = 0u;
    bool          refuse = false;
};

}

TEST(retired_slots, a_retired_slot_is_not_done_until_the_queue_reaches_its_ticket)
{ FakeQueue queue;
  RetiredSlots slots;
  CHECK(!slots.any());
  CHECK(slots.retire(2u, queue));
  CHECK(slots.retired(2u));
  CHECK(slots.any());
  CHECK(!slots.done(2u, queue));
  queue.done = 1u;
  CHECK(slots.done(2u, queue)); }

TEST(retired_slots, slots_retired_later_wait_for_their_own_ticket)
{ FakeQueue queue;
  RetiredSlots slots;
  CHECK(slots.retire(0u, queue));
  CHECK(slots.retire(1u, queue));
  queue.done = 1u;
  CHECK(slots.done(0u, queue));
  CHECK(!slots.done(1u, queue)); }

TEST(retired_slots, a_freed_slot_is_no_longer_retired_or_done)
{ FakeQueue queue;
  RetiredSlots slots;
  CHECK(slots.retire(1u, queue));
  queue.done = 1u;
  slots.freed(1u);
  CHECK(!slots.retired(1u));
  CHECK(!slots.done(1u, queue));
  CHECK(!slots.any()); }

TEST(retired_slots, a_refused_mark_leaves_the_slot_unretired_so_the_caller_must_idle_and_free_it)
{ FakeQueue queue;
  RetiredSlots slots;
  queue.refuse = true;
  CHECK(!slots.retire(3u, queue));
  CHECK(!slots.retired(3u));
  CHECK(!slots.done(3u, queue)); }

TEST(retired_slots, slots_past_the_capacity_are_never_retired)
{ FakeQueue queue;
  RetiredSlots slots;
  CHECK(!slots.retire(RetiredSlots::capacity, queue));
  CHECK(!slots.retired(RetiredSlots::capacity));
  CHECK(!slots.done(RetiredSlots::capacity, queue)); }
