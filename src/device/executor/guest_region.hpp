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
    /* The dma-buf the region was taken as; -1 for one taken as a pointer. */
    int  descriptor() const;

    /* `bytes` at `offset`, or null: for a region that is not there, an offset
     * past its end, a length past its end, and a sum that would wrap. */
    uint8_t *at(uint64_t offset, uint64_t bytes) const;
    uint64_t length() const;

    bool invariants_hold() const;

private:
    friend struct GuestRegionProbe;

    uint8_t *base_;
    uint64_t length_;
    int      descriptor_;
};

}  // namespace av

#endif  // AV_GUEST_REGION_HPP
