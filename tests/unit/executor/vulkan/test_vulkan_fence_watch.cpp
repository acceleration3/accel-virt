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
