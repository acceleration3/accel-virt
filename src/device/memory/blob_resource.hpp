/* One guest blob: guest pages the device holds mapped - a context's ring, its
 * fence page and reply slot, a scanout's framebuffer, a present's pages.
 * Nothing here is ever handed to a host GPU (docs/host-memory.md). */
#ifndef AV_BLOB_RESOURCE_HPP
#define AV_BLOB_RESOURCE_HPP

#include "core/support/status.hpp"
#include "device/memory/guest_page_alias.hpp"
#include "device/memory/guest_page_mapping.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
class ControlRequest;
class Host;

class BlobResource {
public:
    BlobResource(Host &host, uint32_t id);
    BlobResource(const BlobResource &) = delete;
    BlobResource &operator=(const BlobResource &) = delete;

    /* Main loop. The GuestPageMapping's answer, or invalid_parameter when the
     * entries do not cover `size`. */
    [[nodiscard]] Status back_with_guest(ControlRequest &request, size_t offset, uint32_t entries, uint64_t size);

    uint32_t id() const;
    uint64_t size() const;
    /* A context bound to this blob holds a pin, and so does a head showing
     * it; a pinned blob is not released. */
    void     pin();
    void     unpin();
    bool     pinned() const;
    /* The blob's memory for a worker: valid while pinned. */
    BlobWindow window() const;
    /* Main loop, while pinned: the blob's pages as one host range, made the
     * first time it is asked for and kept until the blob goes - what a
     * display surface is drawn from. The alias's refusal otherwise. */
    [[nodiscard]] Status view(const uint8_t *&base);

private:
    Host            &host_;
    uint32_t         id_;
    GuestPageMapping guest_;
    uint32_t         pins_;
    GuestPageAlias   alias_;
};

}  // namespace av

#endif  // AV_BLOB_RESOURCE_HPP
