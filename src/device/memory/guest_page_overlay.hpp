/* The guest pages a process lent the device, under one host blob
 * (docs/host-memory.md, "Application memory"): each run of them, as the guest
 * names them a piece at a time, has its RAM replaced by the blob's dma-buf at
 * the same offset of the blob - the bytes the RAM held copied in first - so
 * the process's own pages and the host GPU's memory are one memory. Restored,
 * each run gets its RAM back holding what the dma-buf held: what the GPU wrote
 * stays in the process's pages after the device lets go. A page lies under
 * one blob at a time (LentPages). Main loop. */
#ifndef AV_GUEST_PAGE_OVERLAY_HPP
#define AV_GUEST_PAGE_OVERLAY_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
class LentPages;
struct GuestPageOverlayProbe;

class GuestPageOverlay {
public:
    static constexpr size_t   entry_size = 16u;
    static constexpr uint64_t page_size = 4096u;

    /* Laid through `host`, its pages claimed in `lent`. */
    GuestPageOverlay(Host &host, LentPages &lent);
    /* restore() */
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
    [[nodiscard]] Status add(ControlRequest &request, size_t offset, uint32_t entries, int fd, uint64_t size);
    /* Every run its RAM again, last first, and lent no more; the record
     * emptied. A run the host would not restore is logged and left as it is. */
    void                 restore();
    /* The bytes laid so far: the blob's offset the next piece starts at. */
    uint64_t             covered() const;
    uint32_t             runs() const;

    bool invariants_hold() const;

private:
    struct Run {
        void    *pointer_;
        uint64_t length_;
        uint64_t address_;
    };

    [[nodiscard]] Status lay_entry(uint64_t address, uint64_t length, int fd, uint64_t size);
    /* One run of guest RAM at `pointer`, guest address `address`, laid. */
    [[nodiscard]] Status lay_run(void *pointer, uint64_t length, uint64_t address, int fd);

    Host               &host_;
    LentPages          &lent_;
    GrowableArray<Run>  runs_;
    uint64_t            covered_;

    friend struct GuestPageOverlayProbe;
};

}  // namespace av

#endif  // AV_GUEST_PAGE_OVERLAY_HPP
