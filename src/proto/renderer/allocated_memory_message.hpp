/* renderer -> QEMU: the answer to allocate_memory. The memory is allocated or
 * not; when it is, its size and caching (AV_MEMORY_*), and its dma-buf beside
 * the message, which QEMU maps into the hostmem region. */
#ifndef AV_ALLOCATED_MEMORY_MESSAGE_HPP
#define AV_ALLOCATED_MEMORY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AllocatedMemoryMessage {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t write_back = 1u;       /* AV_MEMORY_WRITE_BACK */
    static constexpr uint32_t write_combined = 2u;   /* AV_MEMORY_WRITE_COMBINED */

    /* Nothing was allocated. */
    AllocatedMemoryMessage();
    AllocatedMemoryMessage(uint32_t caching, uint64_t size);

    bool     allocated() const;
    uint32_t caching() const;
    uint64_t size() const;
    uint32_t descriptors() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, a caching that is neither, an
     * allocation of no size, anything but zeros when nothing was allocated,
     * or a descriptor count other than descriptors(). Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t caching_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_ALLOCATED_MEMORY_MESSAGE_HPP
