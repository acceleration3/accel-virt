/* The driver's word that a shared object is gone: dxgkrnl destroyed the
 * allocation it was minted over, which it does only once every process that
 * created or opened it has let go. Until then the device holds what was shared
 * under that id, whatever became of the context that shared it
 * (docs/protocol.md). A virtio-gpu control request of this driver's own type:
 * the header, then the object id. */
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

    /* malformed for a request too short to hold the id, or an id of 0, which
     * no object is minted under. */
    [[nodiscard]] Status decode(WireReader &reader);
    uint64_t             object_id() const;

private:
    static constexpr size_t header_size = 24u;

    uint64_t object_id_;
};

}  // namespace av

#endif  // AV_SHARED_RELEASE_REQUEST_HPP
