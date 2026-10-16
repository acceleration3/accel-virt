/* The descriptor update templates a guest made on this context, and how long
 * each one's blob is (feature 047, contracts/descriptor_templates.md).
 *
 * The guest writes the length of a WithTemplate command's blob, and a length
 * from the guest is a claim: the template it names says what the length must
 * be, and this table is what the device checks it against (constitution V). It
 * is per context, decoded on that context's own thread, so it needs no lock. */
#ifndef AV_HOST_TEMPLATE_TABLE_HPP
#define AV_HOST_TEMPLATE_TABLE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "proto/vulkan/descriptor_template_layout.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct HostTemplateTableProbe;

/* One template: the device it belongs to and the bytes one update through it
 * reads. A type of its own so the table's entries are complete
 * (tools/header_scan.py reads a class's members only to its first nested
 * type). */
struct HostTemplate {
    VkDevice device_;
    uint64_t bytes_;
};

class HostTemplateTable {
public:
    explicit HostTemplateTable(BlockSource &blocks);
    HostTemplateTable(const HostTemplateTable &) = delete;
    HostTemplateTable &operator=(const HostTemplateTable &) = delete;

    /* Holds what a template of `device` the guest is about to create reads,
     * sized from the entries it named, with room made for it. malformed for
     * entries whose end leaves 64 bits or a descriptor type a template may not
     * write - the guest sent them, so they are checked here rather than
     * trusted - and no_memory when no room can be made. Before the driver is
     * asked, so a template is never made that no update through it could be
     * checked against. A second reserve replaces one the driver never
     * answered. */
    [[nodiscard]] Status reserve(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *info);
    /* The reserved template's handle, once the driver has answered one; the
     * null handle releases the reservation, which is what a driver's refusal
     * leaves. A handle the driver hands out again is this template now. */
    void                 name(VkDescriptorUpdateTemplate made);
    void                 forget(VkDescriptorUpdateTemplate made);
    /* Every template of `device`, as its destruction does. */
    void                 forget_device(VkDevice device);
    /* Every template and the reservation, whichever device they were for: a
     * closing context's. */
    void                 forget_all();
    /* The bytes one update through `made` reads; 0 for a handle this context
     * never made, which refuses any blob but an empty one. */
    uint64_t             bytes_of(VkDescriptorUpdateTemplate made) const;
    uint32_t             live() const;

    /* Every template is of a device, and a reservation is a device and a size
     * or nothing at all. */
    bool invariants_hold() const;

private:
    KeyedSlots<uint64_t, HostTemplate> templates_;
    /* What reserve held for the template the driver is being asked for. */
    HostTemplate                       reserved_;

    friend struct HostTemplateTableProbe;
};

}  // namespace av

#endif  // AV_HOST_TEMPLATE_TABLE_HPP
