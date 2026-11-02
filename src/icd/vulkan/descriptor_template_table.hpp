#ifndef AV_ICD_DESCRIPTOR_TEMPLATE_TABLE_HPP
#define AV_ICD_DESCRIPTOR_TEMPLATE_TABLE_HPP

#include "core/support/keyed_slots.hpp"
#include "proto/vulkan/descriptor_template_layout.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
class ThreadLock;
}

namespace av::icd {

struct DescriptorTemplateTableProbe;

struct DescriptorTemplateSlot {
    VkDevice                   device_;
    VkDescriptorUpdateTemplate template_;
    uint64_t                   bytes_;
};

class DescriptorTemplateTable {
public:
    DescriptorTemplateTable(BlockSource &memory, ThreadLock &lock);
    DescriptorTemplateTable(const DescriptorTemplateTable &) = delete;
    DescriptorTemplateTable &operator=(const DescriptorTemplateTable &) = delete;

    VkResult record(VkDevice device, VkDescriptorUpdateTemplate made, const VkDescriptorUpdateTemplateCreateInfo *info);
    void     forget(VkDescriptorUpdateTemplate made);
    void     forget_device(VkDevice device);
    uint64_t bytes_of(VkDescriptorUpdateTemplate made) const;
    uint32_t live() const;

    bool invariants_hold() const;

private:
    uint64_t key_of(VkDescriptorUpdateTemplate made) const;

    ThreadLock                                   &lock_;
    KeyedSlots<uint64_t, DescriptorTemplateSlot> slots_;

    friend struct DescriptorTemplateTableProbe;
};

}

#endif
