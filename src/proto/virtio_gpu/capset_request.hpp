/* The capset queries (virtio 1.2 5.7.6.8): GET_CAPSET_INFO by index and
 * GET_CAPSET by id and version, both 32 bytes. */
#ifndef AV_CAPSET_REQUEST_HPP
#define AV_CAPSET_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class CapsetRequest {
public:
    static constexpr size_t encoded_size = 32u;

    CapsetRequest();

    /* From the start of the request; malformed when shorter than 32 bytes. */
    [[nodiscard]] Status decode_info(WireReader &reader);
    [[nodiscard]] Status decode_capset(WireReader &reader);
    uint32_t             index() const;
    uint32_t             capset_id() const;
    uint32_t             capset_version() const;

private:
    static constexpr size_t header_size = 24u;

    void skip_header(WireReader &reader);

    uint32_t index_;
    uint32_t capset_id_;
    uint32_t capset_version_;
};

}  // namespace av

#endif  // AV_CAPSET_REQUEST_HPP
