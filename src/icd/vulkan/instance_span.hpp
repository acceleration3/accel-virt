#ifndef AV_VKICD_INSTANCE_SPAN_HPP
#define AV_VKICD_INSTANCE_SPAN_HPP

#include "guest/kmt/avgpu_adapters.hpp"
#include "core/support/status.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av::icd {

class InstanceSpan {
public:
    static constexpr uint32_t max_instances = AvgpuAdapters::max_adapters;

    explicit InstanceSpan(VkInstance own);
    InstanceSpan(const InstanceSpan &) = delete;
    InstanceSpan &operator=(const InstanceSpan &) = delete;

    [[nodiscard]] Status add(VkInstance sibling);
    bool                 spans_from(VkInstance instance) const;
    uint32_t             count() const;
    VkInstance           at(uint32_t index) const;

    bool invariants_hold() const;

private:
    VkInstance instances_[max_instances];
    uint32_t   count_;
};

}

#endif
