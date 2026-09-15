/* The body of IMPORT_BLOB, the transport command that hands `size` bytes at
 * `offset` of one of a context's guest blobs to its executor as memory of type
 * `memory_type` (docs/protocol.md, Transport commands; feature 045). */
#ifndef AV_BLOB_IMPORT_REQUEST_HPP
#define AV_BLOB_IMPORT_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImportRequest {
public:
    static constexpr size_t   encoded_size = 32u;
    /* What the host GPU imports a host pointer in: whole pages of it. */
    static constexpr uint64_t page_size = 4096u;

    BlobImportRequest();
    BlobImportRequest(uint32_t resource_id, uint64_t offset, uint64_t size, uint32_t memory_type);

    uint32_t resource_id() const;
    uint64_t offset() const;
    uint64_t size() const;
    uint32_t memory_type() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a body of any length but encoded_size - framing, which no
     * producer writes; invalid_parameter for a non-zero reserved field, an
     * offset or size that is not whole pages, no size, or a range past 2^64.
     * Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool range_valid() const;

    uint32_t resource_id_;
    uint64_t offset_;
    uint64_t size_;
    uint32_t memory_type_;
};

}  // namespace av

#endif  // AV_BLOB_IMPORT_REQUEST_HPP
