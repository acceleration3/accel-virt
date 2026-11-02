#ifndef AV_SHARED_RELEASE_REQUEST_HPP
#define AV_SHARED_RELEASE_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class SharedReleaseRequest {
public:
    static constexpr uint32_t type = 0x02F1u;
    static constexpr size_t   encoded_size = 32u;

    SharedReleaseRequest();

    [[nodiscard]] Status decode(WireReader &reader);
    uint64_t             object_id() const;

private:
    static constexpr size_t header_size = 24u;

    uint64_t object_id_;
};

}

#endif
