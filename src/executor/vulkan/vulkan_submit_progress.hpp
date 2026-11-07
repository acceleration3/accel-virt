#ifndef AV_VULKAN_SUBMIT_PROGRESS_HPP
#define AV_VULKAN_SUBMIT_PROGRESS_HPP

#include "executor/vulkan/vulkan_progress.hpp"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class VulkanSubmitProgress final : public VulkanProgress {
public:
    VulkanSubmitProgress();
    VulkanSubmitProgress(const VulkanSubmitProgress &) = delete;
    VulkanSubmitProgress &operator=(const VulkanSubmitProgress &) = delete;

    uint64_t marked() const override;
    uint64_t reached(uint64_t device) override;
    uint64_t reserve(uint64_t device) override;
    void     commit(uint64_t mark, uint64_t queue) override;
    void     cancel(uint64_t mark) override;
    bool     wait_idle(uint64_t device) override;
    bool     wait_marks(uint64_t device, const uint64_t *marks, uint32_t count) override;
    bool     drain(uint64_t device, TeardownBudget &budget) override;
    void     begin_poll() override;
    bool     lost() const override;
    void     forget(VkDevice device);

    uint32_t stuck(uint64_t device) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t slots = 128u;
    static constexpr uint64_t wait_ns = 5000000000u;

    enum class State : uint8_t {
        vacant,
        reserved,
        pending,
        stuck,
        lost
    };

    struct Slot {
        VkDevice device_;
        VkQueue  queue_;
        VkFence  fence_;
        uint64_t mark_;
        State    state_;
    };

    void     settle();
    void     poll_all();
    bool     waited(Slot &slot, uint64_t nanoseconds = wait_ns);
    bool     settled_within(Slot &slot, TeardownBudget &budget);
    bool     queued(Slot &slot);
    bool     settled(Slot &slot);
    uint32_t oldest() const;
    uint32_t vacancy();
    uint32_t reserved_for(uint64_t mark) const;
    bool     fenced(Slot &slot, VkDevice owner);

    Slot     slots_[slots];
    uint64_t marked_;
    uint64_t cached_device_;
    uint64_t cached_reached_;
    bool     cached_;
    bool     stale_;
    bool     lost_;
};

}

#endif
