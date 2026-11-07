#include "executor/vulkan/vulkan_fence_watch.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

void *run_fence_watch(void *watch)
{
    AV_SCOPE("run_fence_watch");
    auto *const made = static_cast<VulkanFenceWatch::Watch *>(watch);
    made->owner_->run(*made);
    return nullptr;
}

}

VulkanFenceWatch::VulkanFenceWatch() : signal_(), watches_{}, stopping_(0u), raised_(0u)
{
    AV_SCOPE("VulkanFenceWatch::VulkanFenceWatch");
}

VulkanFenceWatch::~VulkanFenceWatch()
{
    AV_SCOPE("VulkanFenceWatch::~VulkanFenceWatch");
    settle();
}

void VulkanFenceWatch::bind(void (*signal)(void *context), void *context)
{
    AV_SCOPE("VulkanFenceWatch::bind");
    signal_.bind(signal, context);
}

void VulkanFenceWatch::unbind()
{
    AV_SCOPE("VulkanFenceWatch::unbind");
    settle();
    signal_.clear();
}

bool VulkanFenceWatch::bound() const
{
    AV_SCOPE("VulkanFenceWatch::bound");
    return signal_.bound();
}

bool VulkanFenceWatch::stopping() const
{
    AV_SCOPE("VulkanFenceWatch::stopping");
    return __atomic_load_n(&stopping_, __ATOMIC_ACQUIRE) != 0u;
}

uint32_t VulkanFenceWatch::vacancy()
{
    AV_SCOPE("VulkanFenceWatch::vacancy");
    for (uint32_t slot = 0u; slot < most_watches; slot++) {
        Watch &watch = watches_[slot];
        const uint32_t state = __atomic_load_n(&watch.state_, __ATOMIC_ACQUIRE);
        if (state == finished) {
            pthread_join(watch.thread_, nullptr);
            __atomic_store_n(&watch.state_, idle, __ATOMIC_RELEASE);
        }
        if (__atomic_load_n(&watch.state_, __ATOMIC_ACQUIRE) == idle) return slot;
    }
    return most_watches;
}

bool VulkanFenceWatch::arm(VkDevice device, const VkFence *fences, uint32_t count, VkBool32 wait_all)
{
    AV_SCOPE("VulkanFenceWatch::arm");
    if (!signal_.bound() || count == 0u || count > most_fences) return false;
    const uint32_t slot = vacancy();
    if (slot == most_watches) return false;
    Watch &watch = watches_[slot];
    watch.device_ = device;
    watch.count_ = count;
    watch.all_ = wait_all;
    for (uint32_t at = 0u; at < count; at++) watch.fences_[at] = fences[at];
    watch.owner_ = this;
    __atomic_store_n(&watch.state_, running, __ATOMIC_RELEASE);
    if (pthread_create(&watch.thread_, nullptr, &run_fence_watch, &watch) == 0) return true;
    __atomic_store_n(&watch.state_, idle, __ATOMIC_RELEASE);
    return false;
}

void VulkanFenceWatch::run(Watch &watch)
{
    AV_SCOPE("VulkanFenceWatch::run");
    VkResult waited = VK_TIMEOUT;
    while (waited == VK_TIMEOUT && !stopping())
        waited = ::vkWaitForFences(watch.device_, watch.count_, watch.fences_, watch.all_, slice_ns);
    if (!stopping() && signal_.raise()) __atomic_fetch_add(&raised_, uint64_t{1}, __ATOMIC_RELAXED);
    __atomic_store_n(&watch.state_, finished, __ATOMIC_RELEASE);
}

void VulkanFenceWatch::settle()
{
    AV_SCOPE("VulkanFenceWatch::settle");
    __atomic_store_n(&stopping_, 1u, __ATOMIC_RELEASE);
    for (Watch &watch : watches_) {
        if (__atomic_load_n(&watch.state_, __ATOMIC_ACQUIRE) == idle) continue;
        pthread_join(watch.thread_, nullptr);
        __atomic_store_n(&watch.state_, idle, __ATOMIC_RELEASE);
    }
    __atomic_store_n(&stopping_, 0u, __ATOMIC_RELEASE);
}

uint32_t VulkanFenceWatch::armed() const
{
    AV_SCOPE("VulkanFenceWatch::armed");
    uint32_t counted = 0u;
    for (const Watch &watch : watches_)
        if (__atomic_load_n(&watch.state_, __ATOMIC_ACQUIRE) == running) counted++;
    return counted;
}

uint64_t VulkanFenceWatch::raised() const
{
    AV_SCOPE("VulkanFenceWatch::raised");
    return __atomic_load_n(&raised_, __ATOMIC_RELAXED);
}

bool VulkanFenceWatch::invariants_hold() const
{
    AV_SCOPE("VulkanFenceWatch::invariants_hold");
    for (const Watch &watch : watches_)
        if (watch.state_ != idle && (watch.count_ == 0u || watch.count_ > most_fences)) return false;
    return true;
}

}
