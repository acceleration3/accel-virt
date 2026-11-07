#include "executor/vulkan/vulkan_fence_watch.hpp"
#include "harness/framework/test_registry.hpp"

#include <pthread.h>
#include <time.h>

#include <cstdint>

#include <vulkan/vulkan.h>

using av::VulkanFenceWatch;

namespace {

struct FakeDriver {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  moved = PTHREAD_COND_INITIALIZER;
    bool            signalled = false;
    std::uint32_t   waits = 0u;
    std::uint32_t   waiting = 0u;
    std::uint64_t   last_timeout = 0u;
    VkResult        answer = VK_SUCCESS;
};

FakeDriver driver;

struct FakeTimelines {
    std::uint64_t counters[8] = {};
    std::uint32_t made = 0u;
    std::uint32_t destroyed = 0u;
    std::uint32_t waits = 0u;
    std::uint32_t waiting = 0u;
    std::uint64_t last_timeout = 0u;
    VkSemaphoreWaitFlags last_flags = 0u;
    std::uint32_t last_count = 0u;
};

FakeTimelines timelines;

std::uint32_t slot_of(VkSemaphore semaphore)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(semaphore)) - 1u;
}

VkSemaphore semaphore_at(std::uint32_t slot)
{
    return reinterpret_cast<VkSemaphore>(static_cast<std::uintptr_t>(slot + 1u));
}

void move_timeline(std::uint32_t slot, std::uint64_t value)
{
    pthread_mutex_lock(&driver.lock);
    timelines.counters[slot] = value;
    pthread_cond_broadcast(&driver.moved);
    pthread_mutex_unlock(&driver.lock);
}

struct Raised {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  moved = PTHREAD_COND_INITIALIZER;
    std::uint32_t   count = 0u;
};

Raised raised;

void on_raise(void *)
{
    pthread_mutex_lock(&raised.lock);
    raised.count++;
    pthread_cond_broadcast(&raised.moved);
    pthread_mutex_unlock(&raised.lock);
}

timespec after(std::uint64_t nanoseconds)
{
    timespec at = {};
    clock_gettime(CLOCK_REALTIME, &at);
    at.tv_nsec += static_cast<long>(nanoseconds % 1000000000u);
    at.tv_sec += static_cast<time_t>(nanoseconds / 1000000000u);
    if (at.tv_nsec >= 1000000000L) {
        at.tv_nsec -= 1000000000L;
        at.tv_sec++;
    }
    return at;
}

void reset()
{
    driver.signalled = false;
    driver.waits = 0u;
    driver.waiting = 0u;
    driver.last_timeout = 0u;
    driver.answer = VK_SUCCESS;
    raised.count = 0u;
    timelines = FakeTimelines{};
}

void signal_fence(VkResult answer = VK_SUCCESS)
{
    pthread_mutex_lock(&driver.lock);
    driver.signalled = true;
    driver.answer = answer;
    pthread_cond_broadcast(&driver.moved);
    pthread_mutex_unlock(&driver.lock);
}

bool wait_for_raised(std::uint32_t count)
{
    pthread_mutex_lock(&raised.lock);
    const timespec until = after(5000000000u);
    bool           reached = raised.count >= count;
    while (!reached && pthread_cond_timedwait(&raised.moved, &raised.lock, &until) == 0) reached = raised.count >= count;
    reached = raised.count >= count;
    pthread_mutex_unlock(&raised.lock);
    return reached;
}

bool wait_for_waiting(std::uint32_t count)
{
    pthread_mutex_lock(&driver.lock);
    const timespec until = after(5000000000u);
    bool           reached = driver.waiting >= count;
    while (!reached && pthread_cond_timedwait(&driver.moved, &driver.lock, &until) == 0) reached = driver.waiting >= count;
    reached = driver.waiting >= count;
    pthread_mutex_unlock(&driver.lock);
    return reached;
}

bool wait_for_timeline_waiting(std::uint32_t count)
{
    pthread_mutex_lock(&driver.lock);
    const timespec until = after(5000000000u);
    bool           reached = timelines.waiting >= count;
    while (!reached && pthread_cond_timedwait(&driver.moved, &driver.lock, &until) == 0) reached = timelines.waiting >= count;
    reached = timelines.waiting >= count;
    pthread_mutex_unlock(&driver.lock);
    return reached;
}

VkFence fence_at(std::uintptr_t number)
{
    return reinterpret_cast<VkFence>(number);
}

VkDevice device_of()
{
    return reinterpret_cast<VkDevice>(std::uintptr_t{0x1000u});
}

}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t timeout)
{
    pthread_mutex_lock(&driver.lock);
    driver.waits++;
    driver.waiting++;
    driver.last_timeout = timeout;
    pthread_cond_broadcast(&driver.moved);
    const timespec until = after(timeout);
    while (!driver.signalled && pthread_cond_timedwait(&driver.moved, &driver.lock, &until) == 0) {
    }
    const VkResult result = driver.signalled ? driver.answer : VK_TIMEOUT;
    driver.waiting--;
    pthread_mutex_unlock(&driver.lock);
    return result;
}


VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo *, const VkAllocationCallbacks *,
                                                 VkSemaphore *semaphore)
{
    pthread_mutex_lock(&driver.lock);
    *semaphore = semaphore_at(timelines.made++);
    pthread_mutex_unlock(&driver.lock);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(VkDevice, VkSemaphore, const VkAllocationCallbacks *)
{
    pthread_mutex_lock(&driver.lock);
    timelines.destroyed++;
    pthread_mutex_unlock(&driver.lock);
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreCounterValue(VkDevice, VkSemaphore semaphore, uint64_t *value)
{
    pthread_mutex_lock(&driver.lock);
    *value = timelines.counters[slot_of(semaphore)];
    pthread_mutex_unlock(&driver.lock);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkSignalSemaphore(VkDevice, const VkSemaphoreSignalInfo *info)
{
    move_timeline(slot_of(info->semaphore), info->value);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkWaitSemaphores(VkDevice, const VkSemaphoreWaitInfo *info, uint64_t timeout)
{
    pthread_mutex_lock(&driver.lock);
    timelines.waits++;
    timelines.waiting++;
    timelines.last_timeout = timeout;
    timelines.last_flags = info->flags;
    timelines.last_count = info->semaphoreCount;
    pthread_cond_broadcast(&driver.moved);
    for (;;) {
        std::uint32_t reached = 0u;
        for (std::uint32_t at = 0u; at < info->semaphoreCount; at++)
            if (timelines.counters[slot_of(info->pSemaphores[at])] >= info->pValues[at]) reached++;
        const bool any = (info->flags & VK_SEMAPHORE_WAIT_ANY_BIT) != 0u;
        if (any ? reached != 0u : reached == info->semaphoreCount) break;
        pthread_cond_wait(&driver.moved, &driver.lock);
    }
    timelines.waiting--;
    pthread_mutex_unlock(&driver.lock);
    return VK_SUCCESS;
}

}

TEST(vulkan_fence_watch, a_watch_refuses_to_arm_until_a_signal_is_bound)
{ reset();
  VulkanFenceWatch watch;
  const VkFence fence = fence_at(1u);
  CHECK(!watch.bound());
  CHECK(!watch.arm(device_of(), &fence, 1u, VK_TRUE));
  CHECK_EQ(watch.armed(), 0u);
  watch.bind(&on_raise, nullptr);
  CHECK(watch.bound());
  CHECK(watch.arm(device_of(), &fence, 1u, VK_TRUE));
  signal_fence();
  CHECK(wait_for_raised(1u));
  watch.unbind();
  CHECK(!watch.bound());
  CHECK(watch.invariants_hold()); }

TEST(vulkan_fence_watch, a_signalled_fence_raises_the_signal_once_and_not_before)
{ reset();
  VulkanFenceWatch watch;
  watch.bind(&on_raise, nullptr);
  const VkFence fences[2] = {fence_at(1u), fence_at(2u)};
  CHECK(watch.arm(device_of(), fences, 2u, VK_FALSE));
  CHECK(wait_for_waiting(1u));
  CHECK_EQ(raised.count, 0u);
  CHECK_EQ(watch.armed(), 1u);
  CHECK(driver.last_timeout == VulkanFenceWatch::slice_ns);
  signal_fence();
  CHECK(wait_for_raised(1u));
  watch.settle();
  CHECK_EQ(raised.count, 1u);
  CHECK_EQ(watch.raised(), 1u);
  CHECK_EQ(watch.armed(), 0u);
  CHECK(watch.invariants_hold()); }

TEST(vulkan_fence_watch, a_wait_that_fails_still_wakes_the_guest_to_ask_again)
{ reset();
  VulkanFenceWatch watch;
  watch.bind(&on_raise, nullptr);
  const VkFence fence = fence_at(1u);
  CHECK(watch.arm(device_of(), &fence, 1u, VK_TRUE));
  signal_fence(VK_ERROR_DEVICE_LOST);
  CHECK(wait_for_raised(1u));
  watch.settle(); }

TEST(vulkan_fence_watch, settling_ends_a_watch_that_is_still_waiting_without_raising)
{ reset();
  VulkanFenceWatch watch;
  watch.bind(&on_raise, nullptr);
  const VkFence fence = fence_at(1u);
  CHECK(watch.arm(device_of(), &fence, 1u, VK_TRUE));
  CHECK(wait_for_waiting(1u));
  watch.settle();
  CHECK_EQ(watch.armed(), 0u);
  CHECK_EQ(raised.count, 0u);
  CHECK(watch.arm(device_of(), &fence, 1u, VK_TRUE));
  signal_fence();
  CHECK(wait_for_raised(1u));
  watch.settle(); }

TEST(vulkan_fence_watch, only_so_many_watches_and_fences_are_taken_and_a_finished_one_makes_room)
{ reset();
  VulkanFenceWatch watch;
  watch.bind(&on_raise, nullptr);
  VkFence fences[VulkanFenceWatch::most_fences + 1u] = {};
  for (std::uint32_t at = 0u; at <= VulkanFenceWatch::most_fences; at++) fences[at] = fence_at(at + 1u);
  CHECK(!watch.arm(device_of(), fences, VulkanFenceWatch::most_fences + 1u, VK_TRUE));
  CHECK(!watch.arm(device_of(), fences, 0u, VK_TRUE));
  for (std::uint32_t at = 0u; at < VulkanFenceWatch::most_watches; at++) CHECK(watch.arm(device_of(), fences, 1u, VK_TRUE));
  CHECK(!watch.arm(device_of(), fences, 1u, VK_TRUE));
  CHECK_EQ(watch.armed(), VulkanFenceWatch::most_watches);
  signal_fence();
  CHECK(wait_for_raised(VulkanFenceWatch::most_watches));
  CHECK(watch.arm(device_of(), fences, 1u, VK_TRUE));
  watch.settle();
  CHECK(watch.invariants_hold()); }

TEST(vulkan_fence_watch, a_watch_with_queue_targets_sleeps_on_the_timeline_without_a_timeout_until_it_is_reached)
{ reset();
  VulkanFenceWatch watch;
  watch.bind(&on_raise, nullptr);
  const VkFence fence = fence_at(1u);
  const VulkanFenceWatch::Target target = {semaphore_at(5u), 3u};
  CHECK(watch.arm(device_of(), &fence, 1u, VK_TRUE, &target));
  CHECK(wait_for_timeline_waiting(1u));
  CHECK_EQ(driver.waits, 0u);
  CHECK_EQ(raised.count, 0u);
  CHECK(timelines.last_timeout == UINT64_MAX);
  CHECK((timelines.last_flags & VK_SEMAPHORE_WAIT_ANY_BIT) != 0u);
  CHECK_EQ(timelines.last_count, 2u);
  move_timeline(5u, 3u);
  CHECK(wait_for_waiting(1u));
  signal_fence();
  CHECK(wait_for_raised(1u));
  watch.settle();
  CHECK_EQ(raised.count, 1u);
  CHECK(watch.invariants_hold()); }

TEST(vulkan_fence_watch, settling_wakes_a_targeted_watch_through_the_cancel_timeline_and_raises_nothing)
{ reset();
  VulkanFenceWatch watch;
  watch.bind(&on_raise, nullptr);
  const VkFence fence = fence_at(1u);
  const VulkanFenceWatch::Target target = {semaphore_at(5u), 3u};
  CHECK(watch.arm(device_of(), &fence, 1u, VK_TRUE, &target));
  CHECK(wait_for_timeline_waiting(1u));
  watch.settle();
  CHECK_EQ(watch.armed(), 0u);
  CHECK_EQ(raised.count, 0u);
  CHECK_EQ(timelines.waiting, 0u);
  CHECK(watch.arm(device_of(), &fence, 1u, VK_TRUE, &target));
  CHECK(wait_for_timeline_waiting(1u));
  watch.settle();
  CHECK_EQ(raised.count, 0u);
  watch.forget(device_of());
  CHECK_EQ(timelines.destroyed, 1u);
  CHECK(watch.invariants_hold()); }

TEST(vulkan_fence_watch, a_wait_for_all_re_sleeps_on_the_first_unreached_target)
{ reset();
  VulkanFenceWatch watch;
  watch.bind(&on_raise, nullptr);
  const VkFence fences[2] = {fence_at(1u), fence_at(2u)};
  const VulkanFenceWatch::Target targets[2] = {{semaphore_at(5u), 1u}, {semaphore_at(6u), 1u}};
  CHECK(watch.arm(device_of(), fences, 2u, VK_TRUE, targets));
  CHECK(wait_for_timeline_waiting(1u));
  move_timeline(5u, 1u);
  move_timeline(6u, 1u);
  signal_fence();
  CHECK(wait_for_raised(1u));
  watch.settle();
  CHECK_EQ(raised.count, 1u); }
