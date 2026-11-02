#ifndef AV_BLOB_REQUEST_HPP
#define AV_BLOB_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class BlobRequest {
public:
    BlobRequest();

    [[nodiscard]] Status decode_resource(WireReader &reader);
    [[nodiscard]] Status decode_map(WireReader &reader);
    uint32_t             resource_id() const;
    uint64_t             offset() const;

private:
    static constexpr size_t header_size = 24u;
    static constexpr size_t resource_size = 32u;
    static constexpr size_t map_size = 40u;

    uint32_t resource_id_;
    uint64_t offset_;
};

}

#endif
