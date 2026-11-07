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
    uint64_t reached() override;
    uint64_t mark(uint64_t device, uint64_t queue) override;
    void     wait_idle() override;
    void     forget(VkDevice device);

    bool invariants_hold() const;

private:
    static constexpr uint32_t slots = 128u;
    static constexpr uint64_t wait_ns = 5000000000u;

    struct Slot {
        VkDevice device_;
        VkFence  fence_;
        uint64_t mark_;
        bool     pending_;
    };

    void     settle();
    uint32_t oldest() const;
    uint32_t vacant();

    Slot     slots_[slots];
    uint64_t marked_;
};

}

#endif
