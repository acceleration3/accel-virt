#ifndef AV_BLOB_IMPORT_REQUEST_HPP
#define AV_BLOB_IMPORT_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/submission/blob_role.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImportRequest {
public:
    static constexpr size_t encoded_size = 16u;

    BlobImportRequest();
    explicit BlobImportRequest(uint32_t resource_id);
    BlobImportRequest(uint32_t resource_id, const BlobRole &role);

    uint32_t         resource_id() const;
    const BlobRole &role() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint32_t resource_id_;
    BlobRole role_;
};

}

#endif
