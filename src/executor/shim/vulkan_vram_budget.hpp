#ifndef AV_VULKAN_VRAM_BUDGET_HPP
#define AV_VULKAN_VRAM_BUDGET_HPP

#include "executor/vram_budget_source.hpp"

#include <vulkan/vulkan.h>

namespace av {

class VulkanVramBudget final : public VramBudgetSource {
public:
    VulkanVramBudget();

    void bind(VkPhysicalDevice physical);
    bool device_local(uint64_t &budget, uint64_t &usage) override;

private:
    VkPhysicalDevice physical_;
};

}

#endif
