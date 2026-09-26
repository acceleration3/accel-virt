/* QEMU -> renderer: a blob the guest released. QEMU's BlobTable is the
 * authority on which blobs live; the renderer drops its mirror of this one and
 * any import it made of it. */
#ifndef AV_BLOB_RETIRED_MESSAGE_HPP
#define AV_BLOB_RETIRED_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobRetiredMessage {
public:
    static constexpr size_t   encoded_size = 4u;
    static constexpr uint32_t descriptors = 0u;

    BlobRetiredMessage();
    explicit BlobRetiredMessage(uint32_t blob);

    uint32_t blob() const;

    /* too_large for less room than encoded_size; invalid_parameter for blob 0. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, any descriptor, or blob 0. Unchanged
     * unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint32_t blob_;
};

}  // namespace av

#endif  // AV_BLOB_RETIRED_MESSAGE_HPP
