/* One context's view of host memory it shares with the guest
 * (docs/host-memory.md): an import of its GPU's memory source, read and
 * written through the source's own mapping, bounds-checked.
 *
 * OpenGL's generated decoder asks for a host pointer at a guest offset at more
 * than a thousand sites - it is the bulk-data path, not a corner case - and
 * every offset and length in those asks came off the wire. A region that cannot
 * answer one refuses, and the command that named it is refused with it. */
#ifndef AV_GUEST_REGION_HPP
#define AV_GUEST_REGION_HPP

#include <stdint.h>

namespace av {

class ImportedBlob;
class SourceMemory;

class GuestRegion {
public:
    GuestRegion();
    GuestRegion(const GuestRegion &) = delete;
    GuestRegion &operator=(const GuestRegion &) = delete;

    /* A session's arena, which is its region: one at a time, since a second
     * would leave the first unreachable with what was made over it. The
     * region's address names the import on the wire. False, with the blob
     * untouched, while a region is held or for memory with no mapping. */
    [[nodiscard]] bool adopt(ImportedBlob &blob);
    /* The region given back, if `blob` is the one it was adopted as. */
    void               abandon(ImportedBlob &blob);
    /* The memory the region is, for an API that imports it on the GPU; null
     * for no region. */
    const SourceMemory *memory() const;

    /* `bytes` at `offset`, or null: for a region that is not there, an offset
     * past its end, a length past its end, and a sum that would wrap. */
    uint8_t *at(uint64_t offset, uint64_t bytes) const;
    /* Whether `bytes` points inside the region: memory a decoder resolved
     * through `at`, rather than a copy of its own. */
    bool     holds(const void *bytes) const;
    /* `value` stored at `offset` as one aligned eight-byte store, which the
     * guest reads whole and never half: an error word, a fence value. False
     * for a word outside the region or off its alignment. */
    [[nodiscard]] bool publish(uint64_t offset, uint64_t value);
    uint64_t length() const;

    bool invariants_hold() const;

private:
    friend struct GuestRegionProbe;

    const SourceMemory *memory_;
    uint8_t            *base_;
    uint64_t            length_;
};

}  // namespace av

#endif  // AV_GUEST_REGION_HPP
