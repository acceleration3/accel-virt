/* Which context each host blob was made for (docs/host-memory.md), as a
 * context's imports ask it: a context imports only memory its own process
 * opened. Virtual because the answer lives in two places - the host blob
 * table in QEMU's process, and a copy of it in the renderer's, where the
 * contexts run in renderer mode (docs/renderer.md). Any thread. */
#ifndef AV_HOST_BLOB_OWNERS_HPP
#define AV_HOST_BLOB_OWNERS_HPP

#include <stdint.h>

namespace av {

class HostBlobOwners {
public:
    HostBlobOwners(const HostBlobOwners &) = delete;
    HostBlobOwners &operator=(const HostBlobOwners &) = delete;

    /* Whether host blob `id` lives and was made for context `context`. */
    virtual bool owns(uint32_t context, uint32_t id) = 0;

protected:
    HostBlobOwners() = default;
    ~HostBlobOwners() = default;
};

}  // namespace av

#endif  // AV_HOST_BLOB_OWNERS_HPP
