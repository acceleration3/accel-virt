/* The header framing one batch of ring bytes (docs/protocol.md): the payload
 * that follows it and the fence its execution completes. */
#ifndef AV_BATCH_HEADER_HPP
#define AV_BATCH_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BatchHeader {
public:
    static constexpr size_t encoded_size = 32u;

    BatchHeader();
    BatchHeader(uint32_t length, uint64_t fence_id);
    /* `published_at` is the guest's performance counter when the batch was
     * written: the device runs a batch only after every other Direct3D
     * stream's batches published before it (D3dOrdering). */
    BatchHeader(uint32_t length, uint64_t fence_id, uint64_t published_at);
    uint64_t published_at() const;

    uint32_t length() const;
    uint64_t fence_id() const;
    /* Ring bytes the batch occupies: header and payload, rounded up to 8. */
    uint64_t span() const;
    /* Whether the whole batch can be held by a data area of `capacity` bytes. */
    bool     fits(uint64_t capacity) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Fence 0 is never submitted. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic     = 0x00425641u;  /* "AVB" */
    static constexpr uint64_t alignment = 8u;

    uint32_t length_;
    uint64_t fence_id_;
    uint64_t published_at_;
};

}  // namespace av

#endif  // AV_BATCH_HEADER_HPP
