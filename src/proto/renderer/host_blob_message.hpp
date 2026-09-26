/* QEMU -> renderer: a host blob, whose hostmem memfd - made and placed by
 * QEMU, which keeps the BAR - travels beside the message. The renderer
 * imports it into the host API a context asks for, as it would memory it had
 * allocated itself. */
#ifndef AV_HOST_BLOB_MESSAGE_HPP
#define AV_HOST_BLOB_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HostBlobMessage {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t descriptors = 1u;

    HostBlobMessage();
    HostBlobMessage(uint32_t blob, uint64_t size);

    uint32_t blob() const;
    uint64_t size() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length or descriptor count, a non-zero reserved
     * field, blob 0 (no virtio-gpu resource has it) or an empty blob.
     * Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint32_t blob_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_HOST_BLOB_MESSAGE_HPP
