#ifndef AV_VULKAN_SET_LAYOUTS_HPP
#define AV_VULKAN_SET_LAYOUTS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class BlockSource;

class VulkanSetLayouts {
public:
    explicit VulkanSetLayouts(BlockSource &blocks);
    VulkanSetLayouts(const VulkanSetLayouts &) = delete;
    VulkanSetLayouts &operator=(const VulkanSetLayouts &) = delete;

    [[nodiscard]] Status take_layout(uint64_t made, uint64_t device, const VkDescriptorSetLayoutBinding *bindings,
                                     uint32_t count);
    [[nodiscard]] Status take_pipeline(uint64_t made, uint64_t device, const VkDescriptorSetLayout *layouts,
                                       uint32_t count);
    void                 forget(uint64_t made);
    void                 forget_device(uint64_t device);
    void                 clear();

    const uint32_t *immutable(uint64_t layout, uint32_t &count) const;
    uint64_t        layout_of(uint64_t pipeline, uint32_t set) const;
    uint32_t        count() const;

    bool invariants_hold() const;

private:
    struct Layout {
        Layout(BlockSource &blocks, uint64_t device);

        GrowableArray<uint32_t> bindings_;
        GrowableArray<uint64_t> sets_;
        uint64_t                device_;
    };

    Status keep(uint64_t made, Layout *&layout, uint64_t device);

    BlockSource                    &blocks_;
    ObjectPool<Layout>              pool_;
    KeyedSlots<uint64_t, Layout *>  layouts_;
};

}

#endif
