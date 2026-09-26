/* QEMU -> renderer: one guest RAM region, whose memfd travels beside the
 * message. The renderer maps it once and reaches every guest blob's pages
 * through it, by the region number a RendererExtent names. */
#ifndef AV_REGION_MESSAGE_HPP
#define AV_REGION_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RegionMessage {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t descriptors = 1u;

    RegionMessage();
    RegionMessage(uint32_t region, uint64_t size);

    uint32_t region() const;
    uint64_t size() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length or descriptor count, a non-zero reserved
     * field, a region an extent cannot name, or a size that is empty or past
     * what an offset reaches. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t region_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_REGION_MESSAGE_HPP
