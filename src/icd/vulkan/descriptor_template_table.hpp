/* The descriptor update templates a process made, and how long each one's blob
 * is (feature 047, contracts/descriptor_templates.md).
 *
 * `vkUpdateDescriptorSetWithTemplate` and `vkCmdPushDescriptorSetWithTemplate`
 * carry a `pData` whose length is the template's, not the call's, so the
 * generated serializer asks `descriptor_template_bytes` for it while it is
 * writing the command. That happens on whatever thread called the command, and
 * templates are created and destroyed on other threads, so this table has a
 * lock of its own rather than borrowing the root's - nothing it does can reach
 * back into the root and take the root's. */
#ifndef AV_ICD_DESCRIPTOR_TEMPLATE_TABLE_HPP
#define AV_ICD_DESCRIPTOR_TEMPLATE_TABLE_HPP

#include "proto/vulkan/descriptor_template_layout.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class ThreadLock;
struct DescriptorTemplateTableProbe;

/* One template: the device it belongs to, its handle, and the bytes one update
 * through it reads. A type of its own so the table's slots are complete
 * (tools/header_scan.py reads a class's members only to its first nested
 * type). */
struct DescriptorTemplateSlot {
    VkDevice                   device_;
    VkDescriptorUpdateTemplate template_;
    uint64_t                   bytes_;
};

class DescriptorTemplateTable {
public:
    /* DXVK keeps one template per pipeline layout and vkd3d-proton one per
     * root signature; a process with more than this is not one either builds. */
    static constexpr uint32_t max_templates = 512u;

    explicit DescriptorTemplateTable(ThreadLock &lock);
    DescriptorTemplateTable(const DescriptorTemplateTable &) = delete;
    DescriptorTemplateTable &operator=(const DescriptorTemplateTable &) = delete;

    /* Remembers what `made` reads, from the entries it was created with.
     * VK_ERROR_INITIALIZATION_FAILED for entries whose end leaves 64 bits or a
     * descriptor type a template may not write, VK_ERROR_OUT_OF_HOST_MEMORY
     * when every slot is taken. A handle already known is replaced, which is
     * what a driver reusing an address means. */
    VkResult record(VkDevice device, VkDescriptorUpdateTemplate made, const VkDescriptorUpdateTemplateCreateInfo *info);
    /* A template destroyed, or one this table never made: both leave no slot. */
    void     forget(VkDescriptorUpdateTemplate made);
    /* Every template of `device`, as destroying the device does. */
    void     forget_device(VkDevice device);
    /* The bytes one update through `made` reads; 0 for a handle this table does
     * not know, which sends no descriptors rather than bytes from nowhere. */
    uint64_t bytes_of(VkDescriptorUpdateTemplate made) const;
    uint32_t live() const;

    /* A slot is either empty or names a template of a device, and no handle
     * appears twice. */
    bool invariants_hold() const;

private:
    const DescriptorTemplateSlot *slot_of(VkDescriptorUpdateTemplate made) const;

    ThreadLock            &lock_;
    DescriptorTemplateSlot slots_[max_templates];

    friend struct DescriptorTemplateTableProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_DESCRIPTOR_TEMPLATE_TABLE_HPP
