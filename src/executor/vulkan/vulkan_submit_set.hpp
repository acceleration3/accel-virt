#ifndef AV_VULKAN_SUBMIT_SET_HPP
#define AV_VULKAN_SUBMIT_SET_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class VulkanSubmitSet {
public:
    explicit VulkanSubmitSet(BlockSource &blocks);
    VulkanSubmitSet(const VulkanSubmitSet &) = delete;
    VulkanSubmitSet &operator=(const VulkanSubmitSet &) = delete;

    [[nodiscard]] Status add_event(uint64_t event);
    [[nodiscard]] Status add_wait(uint64_t semaphore);
    [[nodiscard]] Status add_signal(uint64_t semaphore);

    const uint64_t *events() const;
    uint32_t        event_count() const;
    const uint64_t *waits() const;
    uint32_t        wait_count() const;
    const uint64_t *signals() const;
    uint32_t        signal_count() const;

private:
    [[nodiscard]] Status add_unique(GrowableArray<uint64_t> &into, uint64_t value);

    GrowableArray<uint64_t> events_;
    GrowableArray<uint64_t> waits_;
    GrowableArray<uint64_t> signals_;
};

}

#endif
