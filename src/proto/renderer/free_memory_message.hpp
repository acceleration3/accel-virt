/* QEMU -> renderer: the host blob under `key` is released, so the renderer's
 * library lets go of its memory (an import still holding it keeps it) and the
 * renderer forgets who it was made for. */
#ifndef AV_FREE_MEMORY_MESSAGE_HPP
#define AV_FREE_MEMORY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class FreeMemoryMessage {
public:
    static constexpr size_t encoded_size = 8u;

    FreeMemoryMessage();
    explicit FreeMemoryMessage(uint64_t key);

    uint64_t key() const;

    /* too_large for less room than encoded_size; invalid_parameter for key 0. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, any descriptor or key 0. Unchanged
     * unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint64_t key_;
};

}  // namespace av

#endif  // AV_FREE_MEMORY_MESSAGE_HPP
