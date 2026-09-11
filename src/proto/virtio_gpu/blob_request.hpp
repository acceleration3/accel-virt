/* The resource-addressed virtio-gpu requests the device core reads beyond
 * RESOURCE_CREATE_BLOB: RESOURCE_MAP_BLOB (resource, offset in the hostmem
 * region; 40 bytes) and those that name only a resource - UNMAP_BLOB and UNREF
 * (32 bytes), and RESOURCE_CREATE_2D, whose id sits at the same place. */
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

    /* From the start of the request; malformed when shorter than 32 bytes. */
    [[nodiscard]] Status decode_resource(WireReader &reader);
    /* malformed when shorter than 40 bytes. */
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

}  // namespace av

#endif  // AV_BLOB_REQUEST_HPP
