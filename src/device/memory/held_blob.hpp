/* One blob's memory as an importer holds it (feature 045, docs/host-window.md):
 * one contiguous host range - a guest blob's alias over its scattered pages,
 * or a host blob's own mapping of its memfd - and what a dma-buf over part of
 * it is made from. A view the blob table fills: valid while its hold lasts. */
#ifndef AV_HELD_BLOB_HPP
#define AV_HELD_BLOB_HPP

#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"

#include <stdint.h>

namespace av {

class BlobDmaBuf;

class HeldBlob {
public:
    /* Holds nothing. */
    HeldBlob();
    /* `length` bytes at `base`; `pages` its guest extents, or with
     * `host_memory` the host block it is. */
    HeldBlob(void *base, uint64_t length, const BlobWindow &pages, void *host_memory);

    /* The range's first byte; null for nothing held. */
    void    *base() const;
    uint64_t length() const;
    /* `size` bytes from `offset` as one dma-buf in `shared`: over the host
     * block for a host blob, over the guest pages otherwise. What the
     * dma-buf's make answers. */
    [[nodiscard]] Status share(BlobDmaBuf &shared, uint64_t offset, uint64_t size) const;

private:
    void      *base_;
    uint64_t   length_;
    BlobWindow pages_;
    void      *host_memory_;
};

}  // namespace av

#endif  // AV_HELD_BLOB_HPP
