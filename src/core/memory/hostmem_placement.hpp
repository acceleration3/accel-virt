/* Which host-visible allocation holds which part of the device's hostmem
 * region.
 *
 * VidMm owns the segment's address space: it decides at which offset an
 * allocation sits, and the driver learns that offset from the paging operation
 * that makes it resident. What the driver owns is the judgement the device will
 * make of that offset - page-aligned, wholly inside the region, overlapping
 * nothing already mapped (docs/protocol.md) - and the record of who holds what,
 * so an offset that arrives twice, or for a range somebody else holds, is
 * refused here rather than by the device. PASSIVE only: allocation and paging
 * both reach it. */
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

    /* Records that `owner` holds `bytes` from `offset`, as VidMm placed it.
     * invalid_parameter for no owner, no bytes, an offset that is not a page
     * boundary, or a range the region cannot hold; conflict for a range another
     * allocation already holds; invalid_state for an owner that already holds
     * one; no_space when this holds as many as it can. */
    [[nodiscard]] Status take(uint64_t offset, uint64_t bytes, const void *owner);
    /* Forgets `owner`'s placement; one that holds none is left alone. */
    void                 release(const void *owner);
    /* Which allocation holds the byte at `offset`, or null. */
    const void          *holder(uint64_t offset) const;
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    /* Parallel arrays, kept in offset order so overlap is a comparison with a
     * neighbour. */
    uint32_t place_for(uint64_t page, uint64_t pages, bool &clear) const;
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
