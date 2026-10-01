/* One context's view of guest memory, bounds-checked.
 *
 * OpenGL's generated decoder asks for a host pointer at a guest offset at more
 * than a thousand sites - it is the bulk-data path, not a corner case - and
 * every offset and length in those asks came off the wire. A region that cannot
 * answer one refuses, and the command that named it is refused with it. */
#ifndef AV_GUEST_REGION_HPP
#define AV_GUEST_REGION_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ImportedBlob;

class GuestRegion {
public:
    /* Where a region taken as a dma-buf is mapped: a Direct3D 12 heap over
     * part of it wants its address at 64 KiB (D3d12Session::on_CreateHeap12). */
    static constexpr uint64_t base_alignment = 64u * 1024u;

    GuestRegion();
    GuestRegion(const GuestRegion &) = delete;
    GuestRegion &operator=(const GuestRegion &) = delete;

    /* Takes a mapping the device made. A null base, or no length, is a context
     * with no region - every ask then answers null. */
    void take(uint8_t *base, uint64_t length);
    /* Takes a dma-buf of the region, mapping it here at base_alignment: false,
     * with the descriptor still the caller's, when it cannot be mapped. The
     * mapping and the descriptor are this region's until give_back. */
    [[nodiscard]] bool take_shared(int descriptor, uint64_t length);
    void give_back();
    /* A session's blob arena, which is its region: one at a time, since a
     * second would leave the first unreachable with what was made over it.
     * The region's address names the import on the wire. False, with the
     * blob untouched, while a region is held or for a blob that is not a
     * host pointer - OpenGL and OpenCL import no dma-buf. */
    [[nodiscard]] bool adopt_pointer(ImportedBlob &blob);
    /* The same, a dma-buf mapped here too: what Direct3D's sessions import
     * guest pages as for the host GPU (D3d12ArenaHeaps). */
    [[nodiscard]] bool adopt_pointer_or_buffer(ImportedBlob &blob);
    /* The region given back, if `blob` is the one it was adopted as. */
    void               abandon(ImportedBlob &blob);
    /* The dma-buf the region was taken as; -1 for one taken as a pointer. */
    int  descriptor() const;

    /* `bytes` at `offset`, or null: for a region that is not there, an offset
     * past its end, a length past its end, and a sum that would wrap. */
    uint8_t *at(uint64_t offset, uint64_t bytes) const;
    /* `value` stored at `offset` as one aligned eight-byte store, which the
     * guest reads whole and never half: an error word, a fence value. False
     * for a word outside the region or off its alignment. */
    [[nodiscard]] bool publish(uint64_t offset, uint64_t value);
    uint64_t length() const;

    bool invariants_hold() const;

private:
    friend struct GuestRegionProbe;

    /* The region's address recorded as what the blob became. */
    void record(ImportedBlob &blob) const;

    uint8_t *base_;
    uint64_t length_;
    int      descriptor_;
};

}  // namespace av

#endif  // AV_GUEST_REGION_HPP
