/* How many bytes one update through a descriptor update template reads
 * (feature 047, contracts/descriptor_templates.md).
 *
 * `vkUpdateDescriptorSetWithTemplate` and `vkCmdPushDescriptorSetWithTemplate`
 * carry a `pData` with no length of its own: the length belongs to the
 * template, which was described entry by entry when it was created. Both sides
 * of the wire need that number - the guest to send the blob, the device to
 * refuse a blob the template does not account for - so it is computed here,
 * once, from the entries an application gave.
 *
 * The entries are an application's, so every term is hostile: `offset` and
 * `stride` are `size_t` (four bytes in a 32-bit guest), `descriptorCount` is
 * a `uint32_t`, and their products and sums are checked in 64 bits. The
 * descriptor sizes themselves are the same on a 32-bit and a 64-bit process -
 * a non-dispatchable handle is 64 bits either way - so a blob written by one
 * is read correctly by the other. */
#ifndef AV_PROTO_DESCRIPTOR_TEMPLATE_LAYOUT_HPP
#define AV_PROTO_DESCRIPTOR_TEMPLATE_LAYOUT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct DescriptorTemplateLayoutProbe;

class DescriptorTemplateLayout {
public:
    DescriptorTemplateLayout();

    /* The `count` entries of one create info - as many as the application
     * describes: nothing is kept per entry. Unchanged unless ok, so a refused
     * create leaves the layout it had rather than half of one: too_large when
     * an entry's end leaves 64 bits, not_supported for a descriptor type a
     * template may not carry, invalid_parameter for a count with no array. */
    [[nodiscard]] Status take(const VkDescriptorUpdateTemplateEntry *entries, uint32_t count);

    /* The bytes one update reads: the end of the entry that ends last. */
    uint64_t bytes() const;
    uint32_t entries() const;

    /* A layout of no entries reads nothing, and one with entries reads at
     * least one descriptor. */
    bool invariants_hold() const;

private:
    /* The bytes of one descriptor of `type`, or 0 for a type a template may
     * not carry - which includes any value that is not a descriptor type,
     * since the value came from outside. */
    uint64_t descriptor_bytes(int32_t type) const;
    /* Where `entry`'s last descriptor ends; too_large if that leaves 64 bits. */
    [[nodiscard]] Status end_of(const VkDescriptorUpdateTemplateEntry &entry, uint64_t &end) const;

    uint64_t bytes_;
    uint32_t entries_;

    friend struct DescriptorTemplateLayoutProbe;
};

}  // namespace av

#endif  // AV_PROTO_DESCRIPTOR_TEMPLATE_LAYOUT_HPP
