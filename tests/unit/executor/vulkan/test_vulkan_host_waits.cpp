#include "executor/shim/host_heap.hpp"
#include "executor/vulkan/vulkan_host_waits.hpp"
#include "executor/vulkan/vulkan_submit_set.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

TEST(vulkan_host_waits, a_buffer_s_events_are_kept_once_and_forgotten_with_the_buffer)
{ av::HostHeap heap;
  av::VulkanHostWaits waits(heap);
  CHECK(!waits.any());
  CHECK(waits.record(1u, 10u) == av::Status::ok);
  CHECK(waits.record(1u, 10u) == av::Status::ok);
  CHECK(waits.record(1u, 11u) == av::Status::ok);
  CHECK(waits.record(2u, 10u) == av::Status::ok);
  CHECK_EQ(waits.count(), 3u);
  std::uint32_t seen = 0u;
  waits.each(1u, [&](std::uint64_t) { seen++; });
  CHECK_EQ(seen, 2u);
  waits.forget_buffer(1u);
  CHECK(!waits.waits(1u, 10u));
  CHECK(waits.waits(2u, 10u));
  waits.forget_event(10u);
  CHECK(!waits.any());
  CHECK(waits.invariants_hold()); }

TEST(vulkan_host_waits, a_null_buffer_or_event_is_refused_and_a_secondary_s_events_pass_to_its_primary)
{ av::HostHeap heap;
  av::VulkanHostWaits waits(heap);
  CHECK(waits.record(0u, 10u) == av::Status::invalid_parameter);
  CHECK(waits.record(1u, 0u) == av::Status::invalid_parameter);
  CHECK(waits.record(5u, 50u) == av::Status::ok);
  CHECK(waits.inherit(6u, 5u) == av::Status::ok);
  CHECK(waits.waits(6u, 50u));
  waits.clear();
  CHECK(!waits.any()); }

TEST(vulkan_submit_set, events_waits_and_signals_are_kept_once_each_and_zero_is_ignored)
{ av::HostHeap heap;
  av::VulkanSubmitSet set(heap);
  CHECK(set.add_event(1u) == av::Status::ok);
  CHECK(set.add_event(1u) == av::Status::ok);
  CHECK(set.add_event(0u) == av::Status::ok);
  CHECK(set.add_wait(2u) == av::Status::ok);
  CHECK(set.add_signal(3u) == av::Status::ok);
  CHECK(set.add_signal(4u) == av::Status::ok);
  CHECK_EQ(set.event_count(), 1u);
  CHECK_EQ(set.wait_count(), 1u);
  CHECK_EQ(set.signal_count(), 2u);
  CHECK_EQ(set.events()[0], 1u);
  CHECK_EQ(set.waits()[0], 2u);
  CHECK_EQ(set.signals()[1], 4u); }
