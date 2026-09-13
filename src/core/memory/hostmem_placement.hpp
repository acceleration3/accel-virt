/* Where each host-visible allocation sits inside the device's hostmem region.
 *
 * The protocol puts that choice in the guest's hands: RESOURCE_MAP_BLOB carries
 * the offset and the device only judges it - page-aligned, wholly inside the
 * region, overlapping nothing already mapped (docs/protocol.md). This is the
 * driver's side of that judgement, so a refusal from the device is a bug here.
 * PASSIVE only: allocation and paging both reach it. */
#ifndef AV_HOSTMEM_PLACEMENT_HPP
#define AV_HOSTMEM_PLACEMENT_HPP

#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct HostmemPlacementProbe;

class HostmemPlacement {
public:
    /* As many host-visible allocations as can be placed at once. The region is
     * megabytes and a blob is pages, so what runs out first is this. */
    static constexpr uint32_t max_blobs = 64u;
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit HostmemPlacement(Platform &platform);
    HostmemPlacement(const HostmemPlacement &) = delete;
    HostmemPlacement &operator=(const HostmemPlacement &) = delete;

    /* StartDevice, once the segment is offered: how much region there is. */
    void                 open(uint64_t region_bytes);
    /* StopDevice: nothing is placed in a region that is gone. */
    void                 close();

    /* The lowest offset with room for `bytes` rounded up to whole pages.
     * no_space when the region cannot hold it or this holds as many placements
     * as it can; invalid_parameter for no owner or no bytes; invalid_state for
     * an owner that already holds one. `offset` is 0 unless it returns ok. */
    [[nodiscard]] Status place(uint64_t bytes, const void *owner, uint64_t &offset);
    /* Forgets `owner`'s placement; one that holds none is left alone. */
    void                 release(const void *owner);
    /* Which allocation holds the byte at `offset`, or null. */
    const void          *holder(uint64_t offset) const;
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    /* Parallel arrays, kept in offset order so first fit is a walk. */
    bool     fits(uint32_t index, uint64_t pages, uint64_t &offset) const;
    uint32_t index_of(const void *owner) const;
    void     insert(uint32_t at, uint64_t offset, uint64_t pages, const void *owner);

    Platform    &platform_;
    Mutex        mutex_;
    uint64_t     offsets_[max_blobs];
    uint64_t     pages_[max_blobs];
    const void  *owners_[max_blobs];
    uint32_t     count_;
    uint64_t     region_pages_;

    friend struct HostmemPlacementProbe;
};

}  // namespace av

#endif  // AV_HOSTMEM_PLACEMENT_HPP
