#include "bench_timer.hpp"

#if __has_include("executor/vulkan/vulkan_fence_watch.hpp")
#include "executor/vulkan/vulkan_fence_watch.hpp"
#define AV_BENCH_HAS_FENCE_WATCH 1
#endif

#include <pthread.h>
#include <time.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <vulkan/vulkan.h>

namespace {

std::uint64_t now_ns()
{
    timespec at = {};
    clock_gettime(CLOCK_MONOTONIC, &at);
    return static_cast<std::uint64_t>(at.tv_sec) * 1000000000u + static_cast<std::uint64_t>(at.tv_nsec);
}

void nap(std::uint64_t nanoseconds)
{
    timespec span = {};
    span.tv_sec = static_cast<time_t>(nanoseconds / 1000000000u);
    span.tv_nsec = static_cast<long>(nanoseconds % 1000000000u);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &span, nullptr);
}

struct Signal {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  moved = PTHREAD_COND_INITIALIZER;
    bool            signalled = false;
    std::uint64_t   signalled_at = 0u;
    std::uint32_t   woken = 0u;
};

Signal fence;
#ifdef AV_BENCH_HAS_FENCE_WATCH
Signal wake;
#endif

}

#ifdef AV_BENCH_HAS_FENCE_WATCH
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t timeout)
{
    pthread_mutex_lock(&fence.lock);
    timespec until = {};
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += static_cast<long>(timeout % 1000000000u);
    until.tv_sec += static_cast<time_t>(timeout / 1000000000u);
    if (until.tv_nsec >= 1000000000L) {
        until.tv_nsec -= 1000000000L;
        until.tv_sec++;
    }
    while (!fence.signalled && pthread_cond_timedwait(&fence.moved, &fence.lock, &until) == 0) {
    }
    const VkResult result = fence.signalled ? VK_SUCCESS : VK_TIMEOUT;
    pthread_mutex_unlock(&fence.lock);
    return result;
}

namespace {

void raise_wake(void *)
{
    pthread_mutex_lock(&wake.lock);
    wake.woken++;
    wake.signalled_at = now_ns();
    pthread_cond_broadcast(&wake.moved);
    pthread_mutex_unlock(&wake.lock);
}

}
#endif

namespace {

constexpr std::uint64_t spin_ns = 1000000u;
constexpr std::uint64_t rest_ns = 1000000u;
constexpr std::uint32_t rounds = 200u;

void report(const char *name, std::vector<std::uint64_t> &latencies)
{
    std::sort(latencies.begin(), latencies.end());
    const std::uint64_t median = latencies[latencies.size() / 2u];
    const std::uint64_t worst = latencies[latencies.size() * 99u / 100u];
    std::printf("%-58s median %10.1f ns (%8.3f us)  p99 %10.1f ns (%8.3f us)  (%zu waits)\n", name,
                static_cast<double>(median), static_cast<double>(median) / 1000.0, static_cast<double>(worst),
                static_cast<double>(worst) / 1000.0, latencies.size());
}

void *signal_later(void *delay)
{
    nap(*static_cast<std::uint64_t *>(delay));
    pthread_mutex_lock(&fence.lock);
    fence.signalled = true;
    fence.signalled_at = now_ns();
    pthread_cond_broadcast(&fence.moved);
    pthread_mutex_unlock(&fence.lock);
    return nullptr;
}

std::uint64_t observed_polling()
{
    fence.signalled = false;
    std::uint64_t delay = 4000000u + static_cast<std::uint64_t>(std::rand() % 1000) * 1000u;
    pthread_t     signaller;
    pthread_create(&signaller, nullptr, &signal_later, &delay);
    const std::uint64_t began = now_ns();
    for (;;) {
        pthread_mutex_lock(&fence.lock);
        const bool done = fence.signalled;
        pthread_mutex_unlock(&fence.lock);
        if (done) break;
        if (now_ns() - began >= spin_ns) nap(rest_ns);
    }
    const std::uint64_t seen = now_ns();
    pthread_join(signaller, nullptr);
    return seen - fence.signalled_at;
}

#ifdef AV_BENCH_HAS_FENCE_WATCH
std::uint64_t observed_event(av::VulkanFenceWatch &watch)
{
    fence.signalled = false;
    wake.woken = 0u;
    std::uint64_t delay = 4000000u + static_cast<std::uint64_t>(std::rand() % 1000) * 1000u;
    const VkFence handle = reinterpret_cast<VkFence>(std::uintptr_t{1u});
    if (!watch.arm(reinterpret_cast<VkDevice>(std::uintptr_t{2u}), &handle, 1u, VK_TRUE)) std::exit(1);
    pthread_t signaller;
    pthread_create(&signaller, nullptr, &signal_later, &delay);
    pthread_mutex_lock(&wake.lock);
    while (wake.woken == 0u) pthread_cond_wait(&wake.moved, &wake.lock);
    const std::uint64_t seen = now_ns();
    pthread_mutex_unlock(&wake.lock);
    pthread_join(signaller, nullptr);
    watch.settle();
    return seen - fence.signalled_at;
}
#endif

}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::vector<std::uint64_t> latencies;
    for (std::uint32_t round = 0u; round < rounds; round++) latencies.push_back(observed_polling());
    report("guest fence wait wake-up, 1 ms spin then 1 ms rests", latencies);
#ifdef AV_BENCH_HAS_FENCE_WATCH
    av::VulkanFenceWatch watch;
    watch.bind(&raise_wake, nullptr);
    latencies.clear();
    for (std::uint32_t round = 0u; round < rounds; round++) latencies.push_back(observed_event(watch));
    report("guest fence wait wake-up, host watch and driver wake", latencies);
#endif
    return 0;
}
