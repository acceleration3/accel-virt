/* One host blob (docs/host-memory.md): host GPU memory the executor library
 * allocated for one context's process, as a dma-buf this owns, and where in
 * the hostmem region the guest reaches it once mapped. Releasing it frees the
 * library's allocation, which an import still holding the memory keeps alive
 * in the library, so a blob is never refused release. */
#ifndef AV_HOST_BLOB_HPP
#define AV_HOST_BLOB_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"

#include <stdint.h>

namespace av {

class Host;

class HostBlob {
public:
    /* `memory`'s descriptor is this blob's from now on. */
    HostBlob(Host &host, uint32_t context, const AvHostMemory &memory);
    /* Unmapped first if mapped; the descriptor closed. */
    ~HostBlob();
    HostBlob(const HostBlob &) = delete;
    HostBlob &operator=(const HostBlob &) = delete;

    uint32_t context() const;
    uint64_t size() const;
    /* AV_MEMORY_*: how the host maps it, and so how the guest must. */
    uint32_t caching() const;

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
    Host    &host_;
    uint32_t context_;
    int      fd_;
    uint32_t caching_;
    uint64_t size_;
    uint64_t offset_;
    bool     mapped_;
};

}  // namespace av

#endif  // AV_HOST_BLOB_HPP
