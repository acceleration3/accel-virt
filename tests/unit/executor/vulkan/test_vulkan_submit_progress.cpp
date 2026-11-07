#include "executor/vulkan/vulkan_submit_progress.hpp"
#include "harness/framework/test_registry.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace {

constexpr std::uint32_t most_fences = 300u;

struct FakeFence {
    VkDevice device = nullptr;
    bool     alive = false;
    bool     signalled = false;
    bool     lost = false;
};

struct FakeDriver {
    FakeFence     fences[most_fences] = {};
    std::uint32_t made = 0u;
    std::uint32_t destroyed = 0u;
    std::uint32_t resets = 0u;
    std::uint32_t polls = 0u;
    std::uint32_t waits = 0u;
    std::uint32_t submits = 0u;
    bool          create_fails = false;
    bool          submit_fails = false;
    VkFence       last_submitted = VK_NULL_HANDLE;
};

FakeDriver driver;

std::uint32_t index_of(VkFence fence)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fence)) - 1u;
}

VkFence fence_at(std::uint32_t index)
{
    return reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(index) + 1u);
}

VkDevice device_of(std::uint64_t number)
{
    return reinterpret_cast<VkDevice>(number);
}

constexpr std::uint64_t device = 0x1000u;
constexpr std::uint64_t other_device = 0x2000u;
constexpr std::uint64_t queue = 0x3000u;

struct Rig {
    Rig() { driver = FakeDriver{}; }

    std::uint64_t submit(std::uint64_t on = device)
    {
        const std::uint64_t mark = progress.reserve(on);
        if (mark != 0u) progress.commit(mark, queue);
        return mark;
    }
    void signal_oldest()
    {
        for (std::uint32_t index = 0u; index < driver.made; index++)
            if (driver.fences[index].alive && !driver.fences[index].signalled) {
                driver.fences[index].signalled = true;
                return;
            }
    }
    void signal_all()
    {
        for (std::uint32_t index = 0u; index < driver.made; index++) driver.fences[index].signalled = true;
    }

    av::VulkanSubmitProgress progress;
};

}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice owner, const VkFenceCreateInfo *, const VkAllocationCallbacks *,
                                             VkFence *fence)
{
    if (driver.create_fails || driver.made == most_fences) return VK_ERROR_OUT_OF_HOST_MEMORY;
    driver.fences[driver.made] = FakeFence{owner, true, false, false};
    *fence = fence_at(driver.made++);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks *)
{
    driver.fences[index_of(fence)].alive = false;
    driver.destroyed++;
}

VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t count, const VkFence *fences)
{
    for (uint32_t at = 0u; at < count; at++) driver.fences[index_of(fences[at])].signalled = false;
    driver.resets++;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice, VkFence fence)
{
    driver.polls++;
    const FakeFence &state = driver.fences[index_of(fence)];
    if (state.lost) return VK_ERROR_DEVICE_LOST;
    return state.signalled ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t, const VkFence *fences, VkBool32, uint64_t)
{
    driver.waits++;
    const FakeFence &state = driver.fences[index_of(fences[0])];
    if (state.lost) return VK_ERROR_DEVICE_LOST;
    return state.signalled ? VK_SUCCESS : VK_TIMEOUT;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo *, VkFence fence)
{
    driver.submits++;
    driver.last_submitted = fence;
    return driver.submit_fails ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}

}

TEST(vulkan_submit_progress, a_pending_submission_holds_the_device_back_until_its_fence_signals)
{ Rig rig;
  CHECK_EQ(rig.progress.reached(device), 0u);
  CHECK_EQ(rig.submit(), 1u);
  CHECK_EQ(rig.submit(), 2u);
  CHECK_EQ(rig.progress.marked(), 2u);
  CHECK_EQ(driver.submits, 2u);
  CHECK_EQ(rig.progress.reached(device), 0u);
  driver.fences[0].signalled = true;
  rig.progress.begin_poll();
  CHECK_EQ(rig.progress.reached(device), 1u);
  rig.signal_all();
  rig.progress.begin_poll();
  CHECK_EQ(rig.progress.reached(device), 2u);
  CHECK(rig.progress.invariants_hold()); }

TEST(vulkan_submit_progress, a_device_with_no_pending_work_is_not_held_back_by_another_device_s_work)
{ Rig rig;
  CHECK_EQ(rig.submit(device), 1u);
  CHECK_EQ(rig.submit(other_device), 2u);
  CHECK_EQ(rig.progress.reached(device), 0u);
  CHECK_EQ(rig.progress.reached(other_device), 1u);
  driver.fences[0].signalled = true;
  rig.progress.begin_poll();
  CHECK_EQ(rig.progress.reached(device), 2u);
  CHECK_EQ(rig.progress.reached(other_device), 1u); }

TEST(vulkan_submit_progress, the_fences_are_polled_once_per_poll_however_often_progress_is_asked)
{ Rig rig;
  CHECK_EQ(rig.submit(), 1u);
  rig.progress.begin_poll();
  static_cast<void>(rig.progress.reached(device));
  const std::uint32_t polled = driver.polls;
  for (std::uint32_t asked = 0u; asked < 10u; asked++) static_cast<void>(rig.progress.reached(device));
  CHECK_EQ(driver.polls, polled);
  driver.fences[0].signalled = true;
  CHECK_EQ(rig.progress.reached(device), 0u);
  rig.progress.begin_poll();
  CHECK_EQ(rig.progress.reached(device), 1u); }

TEST(vulkan_submit_progress, a_settled_fence_is_reset_and_reused_for_the_next_submission)
{ Rig rig;
  CHECK_EQ(rig.submit(), 1u);
  rig.signal_all();
  rig.progress.begin_poll();
  CHECK_EQ(rig.progress.reached(device), 1u);
  CHECK_EQ(driver.resets, 1u);
  CHECK_EQ(rig.submit(), 2u);
  CHECK_EQ(driver.made, 1u); }

TEST(vulkan_submit_progress, no_fence_for_a_submission_refuses_the_reservation_and_gives_no_mark)
{ Rig rig;
  driver.create_fails = true;
  CHECK_EQ(rig.progress.reserve(device), 0u);
  CHECK_EQ(rig.progress.marked(), 0u);
  CHECK_EQ(driver.submits, 0u);
  CHECK_EQ(rig.progress.reached(device), 0u);
  driver.create_fails = false;
  CHECK_EQ(rig.submit(), 1u); }

TEST(vulkan_submit_progress, a_reservation_gives_the_device_a_pending_mark_before_the_real_submit_and_a_cancel_gives_it_back)
{ Rig rig;
  const std::uint64_t mark = rig.progress.reserve(device);
  CHECK_EQ(mark, 1u);
  CHECK_EQ(rig.progress.reached(device), 0u);
  CHECK_EQ(driver.submits, 0u);
  rig.progress.cancel(mark);
  CHECK_EQ(rig.progress.reached(device), 1u);
  CHECK(rig.progress.wait_idle(device));
  rig.progress.cancel(mark);
  CHECK(rig.progress.invariants_hold()); }

TEST(vulkan_submit_progress, a_submission_whose_fence_cannot_be_queued_holds_its_device_back_until_the_device_is_forgotten)
{ Rig rig;
  driver.submit_fails = true;
  CHECK_EQ(rig.submit(), 1u);
  CHECK_EQ(rig.progress.stuck(device), 1u);
  CHECK_EQ(rig.progress.reached(device), 0u);
  CHECK_EQ(rig.submit(), 2u);
  rig.signal_all();
  rig.progress.begin_poll();
  CHECK_EQ(rig.progress.reached(device), 0u);
  CHECK(!rig.progress.wait_idle(device));
  CHECK_EQ(rig.progress.reached(other_device), 2u);
  rig.progress.forget(device_of(device));
  CHECK_EQ(rig.progress.stuck(device), 0u);
  CHECK_EQ(rig.progress.reached(device), 2u);
  CHECK(rig.progress.wait_idle(device));
  CHECK(rig.progress.invariants_hold()); }

TEST(vulkan_submit_progress, a_hundred_and_twenty_eight_pending_submissions_make_the_next_wait_for_the_oldest_and_refuse_when_it_times_out)
{ Rig rig;
  for (std::uint32_t index = 0u; index < 128u; index++) CHECK_EQ(rig.submit(), index + 1u);
  const std::uint32_t waited = driver.waits;
  CHECK_EQ(rig.progress.reserve(device), 0u);
  CHECK(driver.waits > waited);
  CHECK_EQ(rig.progress.marked(), 128u);
  CHECK_EQ(driver.submits, 128u);
  driver.fences[0].signalled = true;
  CHECK_EQ(rig.progress.reserve(device), 129u);
  CHECK_EQ(driver.made, 128u);
  CHECK(rig.progress.invariants_hold()); }

TEST(vulkan_submit_progress, waiting_for_a_device_succeeds_only_when_every_fence_of_it_signals)
{ Rig rig;
  CHECK_EQ(rig.submit(device), 1u);
  CHECK_EQ(rig.submit(device), 2u);
  CHECK_EQ(rig.submit(other_device), 3u);
  driver.fences[0].signalled = true;
  CHECK(!rig.progress.wait_idle(device));
  CHECK_EQ(rig.progress.reached(device), 1u);
  driver.fences[1].signalled = true;
  CHECK(rig.progress.wait_idle(device));
  CHECK_EQ(rig.progress.reached(device), 3u);
  CHECK(!rig.progress.wait_idle(other_device));
  CHECK_EQ(rig.progress.reached(other_device), 2u); }

TEST(vulkan_submit_progress, a_lost_device_s_work_counts_as_done_for_the_waits_and_the_polls)
{ Rig rig;
  CHECK_EQ(rig.submit(), 1u);
  CHECK_EQ(rig.submit(), 2u);
  driver.fences[0].lost = true;
  CHECK(!rig.progress.wait_idle(device));
  CHECK_EQ(rig.progress.reached(device), 1u);
  driver.fences[1].lost = true;
  rig.progress.begin_poll();
  CHECK_EQ(rig.progress.reached(device), 2u);
  CHECK(rig.progress.wait_idle(device)); }

TEST(vulkan_submit_progress, forgetting_a_device_destroys_its_fences_once_they_are_idle_and_leaks_them_when_they_are_not)
{ Rig rig;
  CHECK_EQ(rig.submit(device), 1u);
  CHECK_EQ(rig.submit(other_device), 2u);
  driver.fences[0].signalled = true;
  rig.progress.forget(device_of(device));
  CHECK_EQ(driver.destroyed, 1u);
  CHECK(!driver.fences[0].alive);
  rig.progress.forget(device_of(other_device));
  CHECK_EQ(driver.destroyed, 1u);
  CHECK(driver.fences[1].alive);
  CHECK_EQ(rig.progress.reached(other_device), 2u);
  CHECK(rig.progress.invariants_hold()); }

TEST(vulkan_submit_progress, a_slot_taken_over_by_another_device_gets_a_fence_of_that_device)
{ Rig rig;
  CHECK_EQ(rig.submit(device), 1u);
  rig.signal_all();
  rig.progress.begin_poll();
  static_cast<void>(rig.progress.reached(device));
  CHECK_EQ(rig.submit(other_device), 2u);
  CHECK_EQ(driver.destroyed, 1u);
  CHECK_EQ(driver.made, 2u);
  CHECK(driver.fences[1].device == device_of(other_device)); }
