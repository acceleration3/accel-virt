/* Which context each host blob was made for, as the renderer knows it
 * (docs/renderer.md, docs/host-memory.md): QEMU keeps the blobs and the BAR,
 * the renderer's library keeps their memory, and the contexts that import it
 * run here, so the renderer records the owner as it allocates and forgets it
 * as it frees. Main loop writes, a context's worker reads. */
#ifndef AV_RENDERER_HOST_BLOBS_HPP
#define AV_RENDERER_HOST_BLOBS_HPP

#include "core/support/status.hpp"
#include "device/memory/host_blob_owners.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <stdint.h>

namespace av {

class Host;

class RendererHostBlobs final : public HostBlobOwners {
public:
    explicit RendererHostBlobs(Host &host);
    ~RendererHostBlobs();
    RendererHostBlobs(const RendererHostBlobs &) = delete;
    RendererHostBlobs &operator=(const RendererHostBlobs &) = delete;

    /* no_memory when the host has no lock to give; invalid_state once done. */
    [[nodiscard]] Status initialize();
    /* Main loop. Blob `id` made for context `context`: invalid_parameter for
     * an id outside HostBlobId's range or context 0. */
    [[nodiscard]] Status adopt(uint32_t id, uint32_t context);
    /* Main loop. Blob `id` gone; nothing for one never adopted. */
    void                 forget(uint32_t id);
    /* Main loop. Every blob gone: QEMU went away. */
    void                 forget_all();
    bool                 owns(uint32_t context, uint32_t id) override;
    uint32_t             live() const;

private:
    Host    &host_;
    void    *lock_;
    /* Each id's context, or 0 for none. */
    uint32_t owners_[HostBlobId::count];
};

}  // namespace av

#endif  // AV_RENDERER_HOST_BLOBS_HPP
