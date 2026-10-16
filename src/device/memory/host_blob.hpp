/* One host blob (docs/host-memory.md): host GPU memory the executor library
 * allocated for one context's process, as a dma-buf this owns, and where in
 * the hostmem region the guest reaches it once mapped - or the guest pages a
 * process lent, which it lies over ("Application memory"). Releasing it frees the
 * library's allocation, which an import still holding the memory keeps alive
 * in the library, so a blob is never refused release. */
#ifndef AV_HOST_BLOB_HPP
#define AV_HOST_BLOB_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/memory/guest_page_overlay.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
class LentPages;

class HostBlob {
public:
    /* `memory`'s descriptor is this blob's from now on. Pages it is laid
     * over are claimed in `lent`. */
    HostBlob(Host &host, LentPages &lent, uint32_t context, const AvHostMemory &memory);
    /* The lent pages given their RAM back, holding what the memory held;
     * unmapped if mapped; the descriptor closed. */
    ~HostBlob();
    HostBlob(const HostBlob &) = delete;
    HostBlob &operator=(const HostBlob &) = delete;

    uint32_t context() const;
    uint64_t size() const;
    /* AV_MEMORY_*: how the host maps it, and so how the guest must. */
    uint32_t caching() const;

    /* Main loop. The memory laid over the next `entries` runs of lent guest
     * pages, named at `offset` of `request` (GuestPageOverlay::add). */
    [[nodiscard]] Status adopt(ControlRequest &request, size_t offset, uint32_t entries);
    /* The bytes of lent pages it lies over. */
    uint64_t             adopted() const;
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
    GuestPageOverlay overlay_;
};

}  // namespace av

#endif  // AV_HOST_BLOB_HPP
