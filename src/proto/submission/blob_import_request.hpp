/* The body of IMPORT_BLOB, the transport command that hands one host blob the
 * context's process opened (docs/host-memory.md) to the context's executor,
 * whole (docs/protocol.md, Transport commands). */
#ifndef AV_BLOB_IMPORT_REQUEST_HPP
#define AV_BLOB_IMPORT_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImportRequest {
public:
    static constexpr size_t encoded_size = 8u;

    BlobImportRequest();
    explicit BlobImportRequest(uint32_t resource_id);

    uint32_t resource_id() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a body of any length but encoded_size - framing, which no
     * producer writes; invalid_parameter for no resource or a non-zero
     * reserved field. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint32_t resource_id_;
};

}  // namespace av

#endif  // AV_BLOB_IMPORT_REQUEST_HPP
