/* QEMU -> renderer: host memory for a host blob (docs/host-memory.md), which
 * the renderer's executor library allocates under `key` for context
 * `context`'s process. The renderer keeps who it was made for, since the
 * contexts that import it run there; it answers with allocated_memory. */
#ifndef AV_ALLOCATE_MEMORY_MESSAGE_HPP
#define AV_ALLOCATE_MEMORY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AllocateMemoryMessage {
public:
    static constexpr size_t encoded_size = 24u;

    AllocateMemoryMessage();
    AllocateMemoryMessage(uint32_t context, uint32_t request, uint64_t key, uint64_t size);

    uint32_t context() const;
    /* A HostMemoryRequest's encoding, which the library judges. */
    uint32_t request() const;
    uint64_t key() const;
    uint64_t size() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length or any descriptor, context 0, key 0 or
     * no size. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t context_;
    uint32_t request_;
    uint64_t key_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_ALLOCATE_MEMORY_MESSAGE_HPP
