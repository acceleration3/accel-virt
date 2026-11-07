#include "core/platform/poll_clock.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/vulkan/vulkan_park_host.hpp"
#include "executor/vulkan/vulkan_parked_submits.hpp"
#include "executor/vulkan/vulkan_submit_set.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>

namespace {

class Host final : public av::VulkanParkHost {
public:
    bool event_unset(std::uint64_t, std::uint64_t event) override { return event < 100u ? unset[event] : false; }
    bool replay(std::uint32_t command, const std::uint8_t *frame, std::size_t bytes) override
    { if (replayed < 16u) { order[replayed] = frame[0]; commands[replayed] = command; sizes[replayed] = bytes; }
      replayed++;
      return replay_ok; }
    void release_mark(std::uint64_t mark) override { released[released_count++] = mark; }
    void lose() override { lost++; }

    bool          unset[100] = {};
    bool          replay_ok = true;
    std::uint32_t replayed = 0u;
    std::uint8_t  order[16] = {};
    std::uint32_t commands[16] = {};
    std::size_t   sizes[16] = {};
    std::uint64_t released[64] = {};
    std::uint32_t released_count = 0u;
    std::uint32_t lost = 0u;
};

class Clock final : public av::PollClock {
public:
    std::uint64_t now_nanoseconds() const override { return now; }
    void          pause(std::uint64_t) override {}
    std::uint64_t now = 0u;
};

struct Rig {
    Rig() : parked(heap, host, clock) {}

    av::Status park(std::uint8_t tag, std::uint64_t queue, std::uint64_t event, std::uint64_t wait = 0u,
                    std::uint64_t signal = 0u, std::uint64_t mark = 0u, std::uint64_t device = 1u)
    { std::uint8_t frame[3] = {tag, 0u, 0u};
      av::WireReader reader(frame, sizeof frame);
      av::VulkanSubmitSet set(heap);
      CHECK(set.add_event(event) == av::Status::ok);
      CHECK(set.add_wait(wait) == av::Status::ok);
      CHECK(set.add_signal(signal) == av::Status::ok);
      const av::VulkanParkedSubmits::Request request{queue, device, mark != 0u ? mark : tag, 0x44u, &reader, &set};
      return parked.park(request); }

    av::HostHeap              heap;
    Host                      host;
    Clock                     clock;
    av::VulkanParkedSubmits   parked;
};

}

TEST(vulkan_parked_submits, a_batch_is_replayed_with_its_own_frame_once_its_events_are_set)
{ Rig rig;
  rig.host.unset[5] = true;
  CHECK(rig.park(7u, 1u, 5u) == av::Status::ok);
  rig.parked.service();
  CHECK_EQ(rig.host.replayed, 0u);
  rig.host.unset[5] = false;
  rig.parked.service();
  CHECK_EQ(rig.host.replayed, 1u);
  CHECK_EQ(rig.host.order[0], 7u);
  CHECK_EQ(rig.host.commands[0], 0x44u);
  CHECK_EQ(rig.host.sizes[0], 3u);
  CHECK_EQ(rig.host.released_count, 1u);
  CHECK_EQ(rig.host.released[0], 7u);
  CHECK(!rig.parked.pending());
  CHECK(rig.parked.invariants_hold()); }

TEST(vulkan_parked_submits, a_later_batch_on_a_queue_waits_for_the_earlier_one_even_with_no_events)
{ Rig rig;
  rig.host.unset[5] = true;
  CHECK(rig.park(1u, 1u, 5u) == av::Status::ok);
  CHECK(rig.park(2u, 1u, 0u) == av::Status::ok);
  CHECK(rig.park(3u, 2u, 0u) == av::Status::ok);
  rig.parked.service();
  CHECK_EQ(rig.host.replayed, 1u);
  CHECK_EQ(rig.host.order[0], 3u);
  rig.host.unset[5] = false;
  rig.parked.service();
  CHECK_EQ(rig.host.replayed, 3u);
  CHECK_EQ(rig.host.order[1], 1u);
  CHECK_EQ(rig.host.order[2], 2u); }

TEST(vulkan_parked_submits, a_waiter_on_a_semaphore_follows_the_parked_batch_that_signals_it)
{ Rig rig;
  rig.host.unset[5] = true;
  CHECK(rig.park(1u, 1u, 5u, 0u, 60u) == av::Status::ok);
  CHECK(rig.park(2u, 2u, 0u, 60u) == av::Status::ok);
  CHECK(rig.parked.behind(3u, nullptr, 0u) == false);
  const std::uint64_t waits[1] = {60u};
  CHECK(rig.parked.behind(3u, waits, 1u));
  CHECK(rig.parked.behind(1u, nullptr, 0u));
  rig.parked.service();
  CHECK_EQ(rig.host.replayed, 0u);
  rig.host.unset[5] = false;
  rig.parked.service();
  CHECK_EQ(rig.host.replayed, 2u);
  CHECK_EQ(rig.host.order[0], 1u);
  CHECK_EQ(rig.host.order[1], 2u); }

TEST(vulkan_parked_submits, a_batch_over_its_second_is_dropped_with_every_other_and_the_device_is_lost)
{ Rig rig;
  rig.host.unset[5] = true;
  CHECK(rig.park(1u, 1u, 5u) == av::Status::ok);
  rig.clock.now = 500u;
  CHECK(rig.park(2u, 2u, 5u) == av::Status::ok);
  rig.clock.now = av::VulkanParkedSubmits::park_nanoseconds - 1u;
  rig.parked.service();
  CHECK_EQ(rig.host.lost, 0u);
  rig.clock.now = av::VulkanParkedSubmits::park_nanoseconds;
  rig.parked.service();
  CHECK_EQ(rig.host.lost, 1u);
  CHECK_EQ(rig.host.replayed, 0u);
  CHECK_EQ(rig.host.released_count, 2u);
  CHECK(!rig.parked.pending()); }

TEST(vulkan_parked_submits, a_batch_held_only_by_an_earlier_one_also_ages_out)
{ Rig rig;
  rig.host.unset[5] = true;
  CHECK(rig.park(1u, 1u, 5u) == av::Status::ok);
  rig.clock.now = 10u;
  CHECK(rig.park(2u, 1u, 0u) == av::Status::ok);
  rig.clock.now = av::VulkanParkedSubmits::park_nanoseconds + 5u;
  rig.parked.service();
  CHECK_EQ(rig.host.lost, 1u); }

TEST(vulkan_parked_submits, a_replay_that_fails_loses_the_device_and_drops_the_rest)
{ Rig rig;
  rig.host.replay_ok = false;
  CHECK(rig.park(1u, 1u, 0u) == av::Status::ok);
  CHECK(rig.park(2u, 2u, 0u) == av::Status::ok);
  rig.parked.service();
  CHECK_EQ(rig.host.lost, 1u);
  CHECK_EQ(rig.host.replayed, 1u);
  CHECK_EQ(rig.host.released_count, 2u);
  CHECK(!rig.parked.pending()); }

TEST(vulkan_parked_submits, dropping_an_event_or_a_device_releases_the_marks_without_replaying)
{ Rig rig;
  rig.host.unset[5] = true;
  rig.host.unset[6] = true;
  CHECK(rig.park(1u, 1u, 5u, 0u, 0u, 11u, 1u) == av::Status::ok);
  CHECK(rig.park(2u, 2u, 6u, 0u, 0u, 12u, 2u) == av::Status::ok);
  CHECK(rig.park(3u, 3u, 7u, 0u, 0u, 13u, 3u) == av::Status::ok);
  rig.parked.drop_event(5u);
  CHECK_EQ(rig.parked.count(), 2u);
  rig.parked.drop_device(2u);
  CHECK_EQ(rig.parked.count(), 1u);
  CHECK_EQ(rig.host.released_count, 2u);
  CHECK_EQ(rig.host.replayed, 0u);
  rig.parked.drop_all();
  CHECK_EQ(rig.host.released_count, 3u);
  CHECK_EQ(rig.host.lost, 0u); }

TEST(vulkan_parked_submits, the_parked_count_is_bounded)
{ Rig rig;
  for (std::uint32_t index = 0u; index < av::VulkanParkedSubmits::most_parked; index++)
    CHECK(rig.park(1u, index + 1u, 0u, 0u, 0u, index + 100u) == av::Status::ok);
  CHECK(rig.park(1u, 90u, 0u) == av::Status::no_memory);
  CHECK_EQ(rig.parked.count(), av::VulkanParkedSubmits::most_parked);
  CHECK(rig.parked.invariants_hold()); }

TEST(vulkan_parked_submits, servicing_again_from_inside_a_replay_does_nothing)
{ Rig rig;
  CHECK(rig.park(1u, 1u, 0u) == av::Status::ok);
  rig.parked.service();
  rig.parked.service();
  CHECK_EQ(rig.host.replayed, 1u); }
