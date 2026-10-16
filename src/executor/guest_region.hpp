/* One context's view of host memory it shares with the guest
 * (docs/host-memory.md): the import of its GPU's memory source that is the
 * context's blob arena, adopted as the GuestBytes every command's offsets
 * resolve in, through the source's own mapping.
 *
 * Direct3D, OpenCL and OpenGL each take their first import as their region,
 * so the rule for which import that is, and what it is named by, is here. */
#ifndef AV_GUEST_REGION_HPP
#define AV_GUEST_REGION_HPP

#include <stdint.h>

namespace av {

class GuestBytes;
class SourceMemory;

class GuestRegion {
public:
    /* `bytes` is what the backend's decoders resolve offsets in; it outlives
     * the region. */
    explicit GuestRegion(GuestBytes &bytes);
    GuestRegion(const GuestRegion &) = delete;
    GuestRegion &operator=(const GuestRegion &) = delete;

    /* A session's arena, which is its region: one at a time, since a second
     * would leave the first unreachable with what was made over it. The
     * region's address is `handle`, what the import is named by on the wire.
     * False while a region is held or for memory with no mapping. */
    [[nodiscard]] bool adopt(const SourceMemory &memory, uint64_t &handle);
    /* The region given back, if it is `memory`. */
    void               abandon(const SourceMemory &memory);
    /* Whether the region is `memory`: the import a backend's first adopt made
     * of it, rather than an object's own. */
    bool               is(const SourceMemory &memory) const;
    /* Whether there is one: the backend's next import is not its region. */
    bool               held() const;

    /* `value` stored at `offset` as one aligned eight-byte store, which the
     * guest reads whole and never half: an error word, a fence value. False
     * for a word outside the region or off its alignment. */
    [[nodiscard]] bool publish(uint64_t offset, uint64_t value);

    bool invariants_hold() const;

private:
    friend struct GuestRegionProbe;

    GuestBytes &bytes_;
};

}  // namespace av

#endif  // AV_GUEST_REGION_HPP
