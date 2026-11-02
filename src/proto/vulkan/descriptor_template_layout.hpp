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

    [[nodiscard]] Status take(const VkDescriptorUpdateTemplateEntry *entries, uint32_t count);

    uint64_t bytes() const;
    uint32_t entries() const;

    bool invariants_hold() const;

private:
    uint64_t descriptor_bytes(int32_t type) const;
    [[nodiscard]] Status end_of(const VkDescriptorUpdateTemplateEntry &entry, uint64_t &end) const;

    uint64_t bytes_;
    uint32_t entries_;

    friend struct DescriptorTemplateLayoutProbe;
};

}

#endif
