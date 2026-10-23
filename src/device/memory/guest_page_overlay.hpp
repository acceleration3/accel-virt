/* The guest pages a process lent a device, under one lending's memory
 * (GuestRamLending, docs/host-memory.md, "Application memory"): each run of
 * them, as the guest names them a piece at a time, has its RAM replaced by
 * the memory's dma-buf at the same offset of it - the bytes the RAM held
 * copied in first - so the process's own pages and the host GPU's memory are
 * one memory. Restored, each run gets its RAM back holding what the dma-buf
 * held: what the GPU wrote stays in the process's pages after the device lets
 * go. A page lies under one lending at a time (LentPages). Main loop.
 *
 * It outlives the device that laid it - another adapter's device may hold the
 * lending still - so it keeps no device: each call names the host to work
 * through. */
#ifndef AV_GUEST_PAGE_OVERLAY_HPP
#define AV_GUEST_PAGE_OVERLAY_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class ControlRequest;
class GuestRamLending;
class Host;
class LentPages;
struct GuestPageOverlayProbe;

class GuestPageOverlay {
public:
    static constexpr size_t   entry_size = 16u;
    static constexpr uint64_t page_size = 4096u;

    /* Its pages are claimed in `lent` for `owner`, the lending it is part of;
     * its records live in `blocks`. */
    GuestPageOverlay(BlockSource &blocks, LentPages &lent, GuestRamLending &owner);
    /* Restored already: a restore needs a host, which a destructor has not
     * got. */
    ~GuestPageOverlay();
    GuestPageOverlay(const GuestPageOverlay &) = delete;
    GuestPageOverlay &operator=(const GuestPageOverlay &) = delete;

    /* The `entries` memory entries at `offset` of `request`, laid over the
     * dma-buf `fd` from where the last piece ended, of the `size` bytes it
     * has. invalid_parameter for no entries, a short list, an entry that is
     * empty, not whole pages, wraps or runs past `size`; conflict for a page
     * already under a blob; not_found for memory that is not mapped;
     * not_supported for memory that is not guest RAM or that the host cannot
     * overlay; no_memory when the host cannot hold the record. Runs laid
     * before a failure stay laid: restore() takes them back. */
    [[nodiscard]] Status add(Host &host, ControlRequest &request, size_t offset, uint32_t entries, int fd,
                             uint64_t size);
    /* Every run its RAM again, last first, and lent no more; the record
     * emptied. A run the host would not restore is logged and left as it is. */
    void                 restore(Host &host);
    /* The bytes laid so far: the blob's offset the next piece starts at. */
    uint64_t             covered() const;
    uint32_t             runs() const;
    /* The entries laid so far, each as the guest named it. */
    uint32_t             extents() const;
    uint64_t             extent_address(uint32_t index) const;
    uint64_t             extent_length(uint32_t index) const;

    bool invariants_hold() const;

private:
    struct Run {
        void    *pointer_;
        uint64_t length_;
        uint64_t address_;
    };

    struct Extent {
        uint64_t address_;
        uint64_t length_;
    };

    [[nodiscard]] Status lay_entry(Host &host, uint64_t address, uint64_t length, int fd, uint64_t size);
    /* One run of guest RAM at `pointer`, guest address `address`, laid. */
    [[nodiscard]] Status lay_run(Host &host, void *pointer, uint64_t length, uint64_t address, int fd);

    LentPages           &lent_;
    GuestRamLending     &owner_;
    GrowableArray<Run>    runs_;
    GrowableArray<Extent> extents_;
    uint64_t              covered_;

    friend struct GuestPageOverlayProbe;
};

}  // namespace av

#endif  // AV_GUEST_PAGE_OVERLAY_HPP
