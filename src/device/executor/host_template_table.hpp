/* The descriptor update templates a guest made on this context, and how long
 * each one's blob is (feature 047, contracts/descriptor_templates.md).
 *
 * The guest writes the length of a WithTemplate command's blob, and a length
 * from the guest is a claim: the template it names says what the length must
 * be, and this table is what the device checks it against (constitution V). It
 * is per context, decoded on that context's own thread, so it needs no lock. */
#ifndef AV_HOST_TEMPLATE_TABLE_HPP
#define AV_HOST_TEMPLATE_TABLE_HPP

#include "proto/vulkan/descriptor_template_layout.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct HostTemplateTableProbe;

/* One template: the device it belongs to, its handle, and the bytes one update
 * through it reads. A type of its own so the table's slots are complete
 * (tools/header_scan.py reads a class's members only to its first nested
 * type). */
struct HostTemplateSlot {
    VkDevice                   device_;
    VkDescriptorUpdateTemplate template_;
    uint64_t                   bytes_;
    /* Taken by `reserve` and cleared by `name`: a slot sized for a template the
     * driver has not answered a handle for yet. */
    bool                       reserved_;
};

class HostTemplateTable {
public:
    /* As many as the ICD holds: a guest that made more than this could not
     * have sized them either. */
    static constexpr uint32_t max_templates = 512u;

    HostTemplateTable();
    HostTemplateTable(const HostTemplateTable &) = delete;
    HostTemplateTable &operator=(const HostTemplateTable &) = delete;

    /* A slot for a template of `device` the guest is about to create, sized
     * from the entries it named. malformed for entries whose end leaves 64
     * bits or a descriptor type a template may not write - the guest sent
     * them, so they are checked here rather than trusted - and no_space when
     * every slot is taken. Reserved before the driver is asked, so a guest
     * sending more templates than this can track is refused rather than given
     * one no update through it could be checked against. */
    [[nodiscard]] Status reserve(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *info);
    /* The reserved slot's handle, once the driver has answered one; the null
     * handle releases the reservation, which is what a driver's refusal
     * leaves. */
    void                 name(VkDescriptorUpdateTemplate made);
    void                 forget(VkDescriptorUpdateTemplate made);
    /* Every template of `device`, as its destruction does. */
    void                 forget_device(VkDevice device);
    /* Every slot, whichever device it was for: a closing context's. */
    void                 forget_all();
    /* The bytes one update through `made` reads; 0 for a handle this context
     * never made, which refuses any blob but an empty one. */
    uint64_t             bytes_of(VkDescriptorUpdateTemplate made) const;
    uint32_t             live() const;

    /* A slot is either empty, reserved for a device, or names a template of
     * one; no handle appears twice, and at most one slot is reserved. */
    bool invariants_hold() const;

private:
    const HostTemplateSlot *slot_of(VkDescriptorUpdateTemplate made) const;
    HostTemplateSlot       *reserved();

    HostTemplateSlot slots_[max_templates];

    friend struct HostTemplateTableProbe;
};

}  // namespace av

#endif  // AV_HOST_TEMPLATE_TABLE_HPP
