/* One host blob (docs/host-memory.md): host GPU memory the executor library
 * allocated for one context's process, as a dma-buf this owns, and where in
 * the hostmem region the guest reaches it once mapped - or the guest pages a
 * process lent, which it lies over ("Application memory"). Releasing it frees the
 * library's allocation, which an import still holding the memory keeps alive
 * in the library, so a blob is never refused release.
 *
 * A blob over lent pages is made before its pages are known: it waits, its
 * memory not yet made, for the first piece of them, which decides whether it
 * makes the memory and lays it under them or joins another adapter's device's
 * lending of the very pages (HostBlobTable::adopt, GuestRamLending). Either
 * way it holds that lending until it is released; the lending, not the blob,
 * has the pages' overlay and the dma-buf they lie over. */
#ifndef AV_HOST_BLOB_HPP
#define AV_HOST_BLOB_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/resources/host_memory_request.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GuestRamLending;
class Host;

class HostBlob {
public:
    /* `memory`'s descriptor is this blob's from now on. */
    HostBlob(Host &host, uint32_t context, const AvHostMemory &memory);
    /* A blob of `size` bytes of `request`'s memory over pages not lent yet:
     * nothing made. */
    HostBlob(Host &host, uint32_t context, uint64_t size, HostMemoryRequest request);
    /* Unmapped if mapped; the descriptor closed, if it has one. */
    ~HostBlob();
    HostBlob(const HostBlob &) = delete;
    HostBlob &operator=(const HostBlob &) = delete;

    uint32_t context() const;
    uint64_t size() const;
    /* AV_MEMORY_*: how the host maps it, and so how the guest must. */
    uint32_t caching() const;

    /* Whether it waits for pages to lie over, with no memory yet. */
    bool                 waiting() const;
    HostMemoryRequest    request() const;
    /* Main loop. The memory it lies over lent pages as: `lending`, which it
     * now holds, made here (`made_here`: the lender, whose dma-buf `memory`
     * names and the lending owns) or joined (`memory` describes what the
     * executor aliased, with no descriptor). */
    void                 lend(GuestRamLending &lending, bool made_here, const AvHostMemory &memory);
    GuestRamLending     *lending() const;
    bool                 lender() const;
    /* Entries of the lending's, as the guest named them, this blob has laid or
     * verified so far. */
    uint32_t             entries_laid() const;
    void                 laid(uint32_t entries);
    /* Main loop. Placed at `offset` of the hostmem region, which the caller
     * judged: invalid_state when already placed, no_memory when the host
     * cannot map it there. */
    [[nodiscard]] Status map(uint64_t offset);
    /* Main loop. invalid_state unless mapped. */
    [[nodiscard]] Status unmap();
    bool                 mapped() const;
    uint64_t             offset() const;
    /* Whether this blob, mapped, covers part of [offset, offset + size). */
    bool                 overlaps(uint64_t offset, uint64_t size) const;

private:
    Host            &host_;
    uint64_t         size_;
    uint64_t         offset_;
    uint32_t         context_;
    int              fd_;
    uint32_t         caching_;
    bool             mapped_;
    bool             waiting_;
    bool             lender_;
    uint32_t         request_;
    uint32_t         laid_;
    GuestRamLending *lending_;
};

}  // namespace av

#endif  // AV_HOST_BLOB_HPP
