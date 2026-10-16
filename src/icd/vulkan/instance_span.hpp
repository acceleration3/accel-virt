/* The instances one application VkInstance spans: one per avgpu adapter, the
 * application's own first - on the adapter the primary display is on - and
 * one of this driver's beside it for every other adapter (AvgpuAdapters).
 *
 * Each adapter's executor is bound to one host GPU and answers one physical
 * device, and a host instance lives in one context, so a physical device of
 * each adapter needs an instance of its own on that adapter's connection. The
 * loader sees only the first; vkEnumeratePhysicalDevices on it answers every
 * instance's devices, each still a child of its own instance on its own
 * connection, and vkDestroyInstance takes them all. */
#ifndef AV_VKICD_INSTANCE_SPAN_HPP
#define AV_VKICD_INSTANCE_SPAN_HPP

#include "guest/kmt/avgpu_adapters.hpp"
#include "core/support/status.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av::icd {

class InstanceSpan {
public:
    /* One per avgpu adapter, as AvgpuAdapters lists at most. */
    static constexpr uint32_t max_instances = AvgpuAdapters::max_adapters;

    explicit InstanceSpan(VkInstance own);
    InstanceSpan(const InstanceSpan &) = delete;
    InstanceSpan &operator=(const InstanceSpan &) = delete;

    /* no_space past max_instances. */
    [[nodiscard]] Status add(VkInstance sibling);
    /* Whether this is the span of the application's `instance`. */
    bool                 spans_from(VkInstance instance) const;
    /* The application's own and every sibling; at(0) is the own. */
    uint32_t             count() const;
    VkInstance           at(uint32_t index) const;

    bool invariants_hold() const;

private:
    VkInstance instances_[max_instances];
    uint32_t   count_;
};

}  // namespace av::icd

#endif  // AV_VKICD_INSTANCE_SPAN_HPP
