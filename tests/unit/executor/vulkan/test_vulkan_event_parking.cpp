#include "core/platform/poll_clock.hpp"
#include "harness/executor/fake_vulkan_driver.hpp"
#include "harness/executor/generous_budget.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/executor/vulkan_wire_rig.hpp"
#include "harness/framework/test_registry.hpp"

namespace {

class FakeClock final : public av::PollClock {
public:
    std::uint64_t now_nanoseconds() const override { return now; }
    void          pause(std::uint64_t) override {}

    std::uint64_t now = 1000u;
};

struct Rig : av::VulkanWireRig {
    Rig() : probe(backend())
    {
        av::fake_vulkan = av::FakeVulkanDriver{};
        av::fake_vulkan.families = 1u;
        av::fake_vulkan.family_flags[0] = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
        probe.use_clock(clock);
        recorder = command_buffer(pool(0u));
    }

    std::uint64_t waiting(std::uint64_t on_event, VkPipelineStageFlags source = VK_PIPELINE_STAGE_HOST_BIT)
    {
        const std::uint64_t buffer = command_buffer(pool_);
        CHECK(wait_events(buffer, &on_event, 1u, source));
        return buffer;
    }

    void reset(std::uint64_t event_handle)
    {
        av::fake_vulkan.event_status[av::fake_event_index(reinterpret_cast<VkEvent>(event_handle))] = VK_EVENT_RESET;
    }

    FakeClock                 clock;
    av::VulkanBackendProbe    probe;
    std::uint64_t             recorder = 0u;
    std::uint64_t             pool_ = 0u;
};

struct Armed : Rig {
    Armed()
    {
        pool_ = pool(0u);
        event_handle = event();
        reset(event_handle);
    }

    std::uint64_t event_handle = 0u;
};

}

TEST(vulkan_event_parking, a_submit_that_waits_on_a_reset_host_event_parks_and_still_answers_success)
{ Armed rig;
  const std::uint64_t buffer = rig.waiting(rig.event_handle);
  const std::uint64_t fence = rig.fence();
  CHECK(rig.submit(rig.queue, buffer, fence));
  CHECK_EQ(av::fake_vulkan.submits, 0u);
  CHECK_EQ(rig.probe.parked().count(), 1u);
  CHECK(!rig.backend().lost()); }

TEST(vulkan_event_parking, vkSetEvent_releases_the_parked_batch_with_its_own_fence_and_only_then)
{ Armed rig;
  const std::uint64_t buffer = rig.waiting(rig.event_handle);
  const std::uint64_t fence = rig.fence();
  CHECK(rig.submit(rig.queue, buffer, fence));
  CHECK(av::fake_vulkan.submitted_fence[0] == nullptr);
  CHECK(rig.set_event(rig.event_handle));
  CHECK_EQ(av::fake_vulkan.events_set, 1u);
  CHECK_EQ(av::fake_vulkan.submits, 1u);
  CHECK(av::fake_vulkan.submitted_fence[0] == reinterpret_cast<VkFence>(fence));
  CHECK(av::fake_vulkan.submitted_buffer[0] == reinterpret_cast<VkCommandBuffer>(buffer));
  CHECK_EQ(rig.probe.parked().count(), 0u);
  CHECK(!rig.backend().lost()); }

TEST(vulkan_event_parking, a_host_event_that_is_already_set_submits_at_once)
{ Armed rig;
  av::fake_vulkan.event_status[av::fake_event_index(reinterpret_cast<VkEvent>(rig.event_handle))] = VK_EVENT_SET;
  CHECK(rig.submit(rig.queue, rig.waiting(rig.event_handle), rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 1u);
  CHECK_EQ(rig.probe.parked().count(), 0u); }

TEST(vulkan_event_parking, a_wait_whose_source_stage_is_not_the_host_never_asks_the_event_and_never_parks)
{ Armed rig;
  const std::uint64_t buffer = rig.waiting(rig.event_handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  CHECK(rig.submit(rig.queue, buffer, rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 1u);
  CHECK_EQ(av::fake_vulkan.event_polls, 0u); }

TEST(vulkan_event_parking, an_ordinary_submit_asks_no_event_and_parks_nothing)
{ Armed rig;
  CHECK(rig.submit(rig.queue, rig.recorder, rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 1u);
  CHECK_EQ(av::fake_vulkan.event_polls, 0u);
  CHECK(!rig.probe.host_waits().any()); }

TEST(vulkan_event_parking, a_submit_without_host_waits_asks_no_event_even_while_another_buffer_waits_on_one)
{ Armed rig;
  static_cast<void>(rig.waiting(rig.event_handle));
  CHECK(rig.submit(rig.queue, rig.recorder, rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 1u);
  CHECK_EQ(av::fake_vulkan.event_polls, 0u); }

TEST(vulkan_event_parking, later_batches_on_the_same_queue_park_behind_and_release_in_order)
{ Armed rig;
  const std::uint64_t first = rig.waiting(rig.event_handle);
  const std::uint64_t second = rig.command_buffer(rig.pool_);
  CHECK(rig.submit(rig.queue, first, rig.fence()));
  CHECK(rig.submit(rig.queue, second, rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 0u);
  CHECK_EQ(rig.probe.parked().count(), 2u);
  CHECK(rig.set_event(rig.event_handle));
  CHECK_EQ(av::fake_vulkan.submits, 2u);
  CHECK(av::fake_vulkan.submitted_buffer[0] == reinterpret_cast<VkCommandBuffer>(first));
  CHECK(av::fake_vulkan.submitted_buffer[1] == reinterpret_cast<VkCommandBuffer>(second)); }

TEST(vulkan_event_parking, a_batch_on_another_queue_is_not_held_by_a_parked_one)
{ Armed rig;
  CHECK(rig.submit(rig.queue, rig.waiting(rig.event_handle), rig.fence()));
  CHECK(rig.submit(rig.other_queue, rig.recorder, rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 1u);
  CHECK(av::fake_vulkan.submitted_queue[0] == reinterpret_cast<VkQueue>(rig.other_queue)); }

TEST(vulkan_event_parking, a_batch_waiting_on_a_semaphore_a_parked_batch_signals_parks_too_and_follows_it)
{ Armed rig;
  const std::uint64_t semaphore = rig.semaphore();
  const std::uint64_t buffer = rig.waiting(rig.event_handle);
  CHECK(rig.submit(rig.queue, buffer, rig.fence(), nullptr, 0u, &semaphore, 1u));
  CHECK(rig.submit(rig.other_queue, rig.recorder, rig.fence(), &semaphore, 1u));
  CHECK_EQ(av::fake_vulkan.submits, 0u);
  CHECK_EQ(rig.probe.parked().count(), 2u);
  CHECK(rig.set_event(rig.event_handle));
  CHECK_EQ(av::fake_vulkan.submits, 2u);
  CHECK(av::fake_vulkan.submitted_queue[0] == reinterpret_cast<VkQueue>(rig.queue));
  CHECK(av::fake_vulkan.submitted_queue[1] == reinterpret_cast<VkQueue>(rig.other_queue)); }

TEST(vulkan_event_parking, a_batch_parked_for_a_second_is_dropped_unsubmitted_and_the_device_is_lost)
{ Armed rig;
  CHECK(rig.submit(rig.queue, rig.waiting(rig.event_handle), rig.fence()));
  rig.clock.now += av::VulkanParkedSubmits::park_nanoseconds - 1u;
  CHECK(rig.backend().poll());
  CHECK(!rig.backend().lost());
  rig.clock.now += 1u;
  static_cast<void>(rig.backend().poll());
  CHECK(rig.backend().lost());
  CHECK_EQ(rig.probe.parked().count(), 0u);
  CHECK_EQ(av::fake_vulkan.submits, 0u); }

TEST(vulkan_event_parking, the_timeout_is_found_by_the_next_command_without_any_poll)
{ Armed rig;
  CHECK(rig.submit(rig.queue, rig.waiting(rig.event_handle), rig.fence()));
  rig.clock.now += av::VulkanParkedSubmits::park_nanoseconds;
  CHECK(rig.submit(rig.other_queue, rig.recorder, rig.fence()));
  CHECK(rig.backend().lost());
  CHECK_EQ(av::fake_vulkan.submits, 0u); }

TEST(vulkan_event_parking, destroying_the_event_drops_its_batches_without_submitting_or_losing_the_device)
{ Armed rig;
  CHECK(rig.submit(rig.queue, rig.waiting(rig.event_handle), rig.fence()));
  CHECK(rig.destroy_event(rig.event_handle));
  CHECK_EQ(rig.probe.parked().count(), 0u);
  CHECK_EQ(av::fake_vulkan.submits, 0u);
  CHECK(!rig.backend().lost()); }

TEST(vulkan_event_parking, closing_the_context_drops_parked_batches_without_submitting_and_drains)
{ Armed rig;
  CHECK(rig.submit(rig.queue, rig.waiting(rig.event_handle), rig.fence()));
  av::GenerousBudget budget;
  CHECK(rig.backend().drain(budget.budget()));
  CHECK_EQ(rig.probe.parked().count(), 0u);
  CHECK_EQ(av::fake_vulkan.submits, 0u); }

TEST(vulkan_event_parking, a_dependency_with_no_barrier_or_a_host_barrier_is_host_settable_and_another_is_not)
{ Armed rig;
  const std::uint64_t quiet = rig.command_buffer(rig.pool_);
  const std::uint64_t barred = rig.command_buffer(rig.pool_);
  const std::uint64_t bare = rig.command_buffer(rig.pool_);
  CHECK(rig.wait_events2(quiet, rig.event_handle, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, true));
  CHECK(rig.wait_events2(barred, rig.event_handle, VK_PIPELINE_STAGE_2_HOST_BIT, true));
  CHECK(rig.wait_events2(bare, rig.event_handle, 0u, false));
  CHECK(!rig.probe.host_waits().waits(quiet, rig.event_handle));
  CHECK(rig.probe.host_waits().waits(barred, rig.event_handle));
  CHECK(rig.probe.host_waits().waits(bare, rig.event_handle));
  CHECK(rig.submit(rig.queue, quiet, rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 1u);
  CHECK(rig.submit(rig.other_queue, barred, rig.fence()));
  CHECK_EQ(rig.probe.parked().count(), 1u); }

TEST(vulkan_event_parking, a_parked_batch_keeps_its_buffers_pinned_until_it_is_released_or_dropped)
{ Armed rig;
  const std::uint64_t buffer = rig.waiting(rig.event_handle);
  CHECK(rig.submit(rig.queue, buffer, rig.fence()));
  CHECK(rig.probe.guard().mark_of(buffer, rig.device) != 0u);
  CHECK(rig.probe.guard().reached(rig.device) < rig.probe.guard().mark_of(buffer, rig.device));
  CHECK(rig.set_event(rig.event_handle));
  CHECK(rig.probe.guard().invariants_hold()); }

TEST(vulkan_event_parking, beginning_a_buffer_again_forgets_the_events_it_waited_on)
{ Armed rig;
  const std::uint64_t buffer = rig.waiting(rig.event_handle);
  CHECK(rig.probe.host_waits().waits(buffer, rig.event_handle));
  CHECK(rig.begin(buffer));
  CHECK(!rig.probe.host_waits().waits(buffer, rig.event_handle));
  CHECK(rig.submit(rig.queue, buffer, rig.fence()));
  CHECK_EQ(av::fake_vulkan.submits, 1u); }
