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

VulkanFenceWatch::VulkanFenceWatch() : signal_(), watches_{}, cancels_{}, stopping_(0u), raised_(0u)
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

VulkanFenceWatch::Cancel *VulkanFenceWatch::cancel_for(VkDevice device)
{
    AV_SCOPE("VulkanFenceWatch::cancel_for");
    Cancel *spare = nullptr;
    for (Cancel &cancel : cancels_) {
        if (cancel.semaphore_ != VK_NULL_HANDLE && cancel.device_ == device) return &cancel;
        if (cancel.semaphore_ == VK_NULL_HANDLE && spare == nullptr) spare = &cancel;
    }
    if (spare == nullptr) return nullptr;
    VkSemaphoreTypeCreateInfo type = {};
    type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    create.pNext = &type;
    if (::vkCreateSemaphore(device, &create, nullptr, &spare->semaphore_) != VK_SUCCESS) {
        spare->semaphore_ = VK_NULL_HANDLE;
        return nullptr;
    }
    spare->device_ = device;
    spare->next_ = 1u;
    return spare;
}

void VulkanFenceWatch::forget(VkDevice device)
{
    AV_SCOPE("VulkanFenceWatch::forget");
    settle();
    for (Cancel &cancel : cancels_) {
        if (cancel.semaphore_ == VK_NULL_HANDLE || cancel.device_ != device) continue;
        ::vkDestroySemaphore(device, cancel.semaphore_, nullptr);
        cancel = Cancel{};
    }
}

bool VulkanFenceWatch::arm(VkDevice device, const VkFence *fences, uint32_t count, VkBool32 wait_all,
                           const Target *targets)
{
    AV_SCOPE("VulkanFenceWatch::arm");
    if (!signal_.bound() || count == 0u || count > most_fences) return false;
    Cancel *cancel = nullptr;
    if (targets != nullptr) {
        bool mapped = true;
        for (uint32_t at = 0u; at < count; at++) mapped = mapped && targets[at].semaphore_ != VK_NULL_HANDLE;
        if (mapped) cancel = cancel_for(device);
    }
    const uint32_t slot = vacancy();
    if (slot == most_watches) return false;
    Watch &watch = watches_[slot];
    watch.device_ = device;
    watch.count_ = count;
    watch.all_ = wait_all;
    for (uint32_t at = 0u; at < count; at++) watch.fences_[at] = fences[at];
    watch.owner_ = this;
    watch.targeted_ = cancel != nullptr;
    watch.cancel_ = cancel != nullptr ? cancel->semaphore_ : VK_NULL_HANDLE;
    watch.cancel_at_ = cancel != nullptr ? cancel->next_ : 0u;
    if (cancel != nullptr)
        for (uint32_t at = 0u; at < count; at++) watch.targets_[at] = targets[at];
    __atomic_store_n(&watch.state_, running, __ATOMIC_RELEASE);
    if (pthread_create(&watch.thread_, nullptr, &run_fence_watch, &watch) == 0) return true;
    __atomic_store_n(&watch.state_, idle, __ATOMIC_RELEASE);
    return false;
}

void VulkanFenceWatch::wait_timelines(Watch &watch)
{
    AV_SCOPE("VulkanFenceWatch::wait_timelines");
    for (;;) {
        VkSemaphore semaphores[most_fences + 1u];
        uint64_t    values[most_fences + 1u];
        uint32_t    waiting = 0u;
        for (uint32_t at = 0u; at < watch.count_; at++) {
            const Target &target = watch.targets_[at];
            uint32_t      seen = 0u;
            while (seen < waiting && semaphores[seen] != target.semaphore_) seen++;
            if (seen < waiting) {
                if (watch.all_ == VK_FALSE ? target.value_ < values[seen] : target.value_ > values[seen])
                    values[seen] = target.value_;
                continue;
            }
            semaphores[waiting] = target.semaphore_;
            values[waiting++] = target.value_;
        }
        if (watch.all_ == VK_TRUE) {
            uint32_t first = waiting;
            for (uint32_t at = 0u; at < waiting && first == waiting; at++) {
                uint64_t counter = 0u;
                if (::vkGetSemaphoreCounterValue(watch.device_, semaphores[at], &counter) != VK_SUCCESS) return;
                if (counter < values[at]) first = at;
            }
            if (first == waiting) return;
            semaphores[0] = semaphores[first];
            values[0] = values[first];
            waiting = 1u;
        }
        semaphores[waiting] = watch.cancel_;
        values[waiting++] = watch.cancel_at_;
        VkSemaphoreWaitInfo wait = {};
        wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait.flags = VK_SEMAPHORE_WAIT_ANY_BIT;
        wait.semaphoreCount = waiting;
        wait.pSemaphores = semaphores;
        wait.pValues = values;
        if (::vkWaitSemaphores(watch.device_, &wait, UINT64_MAX) != VK_SUCCESS || stopping() || watch.all_ == VK_FALSE)
            return;
    }
}

void VulkanFenceWatch::run(Watch &watch)
{
    AV_SCOPE("VulkanFenceWatch::run");
    if (watch.targeted_) wait_timelines(watch);
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
    bool targeted = false;
    for (const Watch &watch : watches_)
        targeted = targeted || (__atomic_load_n(&watch.state_, __ATOMIC_ACQUIRE) != idle && watch.targeted_);
    for (Cancel &cancel : cancels_) {
        if (!targeted || cancel.semaphore_ == VK_NULL_HANDLE) continue;
        VkSemaphoreSignalInfo signal = {};
        signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal.semaphore = cancel.semaphore_;
        signal.value = cancel.next_;
        if (::vkSignalSemaphore(cancel.device_, &signal) == VK_SUCCESS) cancel.next_++;
    }
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
