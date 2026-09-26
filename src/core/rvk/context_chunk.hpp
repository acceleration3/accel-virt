/* One chunk slot of an rvk context, and the resource of the context's range it
 * holds. A chunk is one of two kinds: guest RAM a process allocated, which the
 * host GPU imports as a GUEST blob (feature 045) - the allocation stays the
 * process's, and this names it - or a run of pages of the device's host window
 * the driver placed a host blob in and mapped into the process
 * (docs/host-window.md), which this remembers so it can be taken down. */
#ifndef AV_CONTEXT_CHUNK_HPP
#define AV_CONTEXT_CHUNK_HPP

#include <stdint.h>

namespace av {

class Allocation;

class ContextChunk {
public:
    ContextChunk();

    /* `resource_id` is never 0: that is how a free slot is told apart. */
    void              take_guest(const Allocation &allocation, uint32_t resource_id);
    /* `pages` of the window from page `first`, and the platform's record of
     * their mapping into the process. */
    void              take_window(uint32_t resource_id, uint32_t first, uint32_t pages, void *mapping);
    void              clear();
    bool              bound() const;
    bool              in_window() const;
    /* The guest chunk's allocation; null for a window chunk. */
    const Allocation *allocation() const;
    uint32_t          resource_id() const;
    /* A window chunk's first page, page count and process mapping; zero and
     * null for a guest chunk. */
    uint32_t          first_page() const;
    uint32_t          pages() const;
    void             *mapping() const;

    bool invariants_hold() const;

private:
    friend struct ContextChunkProbe;

    const Allocation *allocation_;
    void             *mapping_;
    uint32_t          resource_id_;
    uint32_t          first_page_;
    uint32_t          pages_;
};

}  // namespace av

#endif  // AV_CONTEXT_CHUNK_HPP
