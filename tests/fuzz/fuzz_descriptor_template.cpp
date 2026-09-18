/* A descriptor update template's entries, from attacker-chosen bytes (feature
 * 047). An application describes a template once and then sends blobs sized by
 * it, so a length this answers is a length something else will read: it may
 * never wrap, never exceed what the entries say, and never be answered for a
 * template it refused. */
#include "proto/vulkan/descriptor_template_layout.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t max_read = av::DescriptorTemplateLayout::max_entries + 1u;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader                  reader(data, size);
    VkDescriptorUpdateTemplateEntry entries[max_read] = {};
    const std::uint32_t             count = reader.read_u8() % (max_read + 1u);
    for (std::uint32_t index = 0u; index < count; index++) {
        VkDescriptorUpdateTemplateEntry &entry = entries[index];
        entry.dstBinding = reader.read_u32();
        entry.dstArrayElement = reader.read_u32();
        entry.descriptorCount = reader.read_u32();
        entry.descriptorType = static_cast<VkDescriptorType>(reader.read_u32());
        entry.offset = static_cast<std::size_t>(reader.read_u64());
        entry.stride = static_cast<std::size_t>(reader.read_u64());
    }
    av::DescriptorTemplateLayout layout;
    const av::Status             taken = layout.take(count != 0u ? entries : nullptr, count);
    if (!layout.invariants_hold()) __builtin_trap();
    if (taken != av::Status::ok) {
        /* A refused template is the one it had: the empty one. */
        if (layout.bytes() != 0u || layout.entries() != 0u) __builtin_trap();
        return 0;
    }
    if (layout.entries() != count) __builtin_trap();
    /* Every accepted entry ends inside the length it answered, computed the
     * other way round: in a wider type, without the overflow checks. */
    for (std::uint32_t index = 0u; index < count; index++) {
        const VkDescriptorUpdateTemplateEntry &entry = entries[index];
        if (entry.descriptorCount == 0u) continue;
        const __uint128_t offset = entry.offset;
        const __uint128_t count_wide = entry.descriptorCount;
        const __uint128_t stride = entry.stride;
        const bool        inline_block = entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
        const __uint128_t end = inline_block ? offset + count_wide
                                             : offset + (count_wide - 1u) * stride + 1u;
        if (end > static_cast<__uint128_t>(layout.bytes())) __builtin_trap();
    }
    return 0;
}
