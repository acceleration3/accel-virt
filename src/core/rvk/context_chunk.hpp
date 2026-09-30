/* One chunk slot of an rvk context, and the resource of the context's range it
 * holds. A chunk is one of two kinds, both mapped into the process by the
 * driver: a run of pages of the device's host window the driver placed a host
 * blob in (docs/host-window.md), or - when the window has no room - guest RAM
 * the driver owns, which the host GPU imports as a GUEST blob (feature 045).
 * This remembers which, so the chunk can be taken down. */
#ifndef AV_CONTEXT_CHUNK_HPP
#define AV_CONTEXT_CHUNK_HPP

#include <stdint.h>

namespace av {

class GuestPages;

class ContextChunk {
public:
    ContextChunk();

    /* `resource_id` is never 0: that is how a free slot is told apart. */
    void              take_ram(uint32_t resource_id, GuestPages &pages);
    /* `pages` of the window from page `first`, and the platform's record of
     * their mapping into the process. */
    void              take_window(uint32_t resource_id, uint32_t first, uint32_t pages, void *mapping);
    void              clear();
    bool              bound() const;
    bool              in_window() const;
    /* The RAM chunk's pages; null for a window chunk. */
    GuestPages       *ram() const;
    uint32_t          resource_id() const;
    /* A window chunk's first page, page count and process mapping; zero and
     * null for a RAM chunk. */
    uint32_t          first_page() const;
    uint32_t          pages() const;
    void             *mapping() const;

    bool invariants_hold() const;

private:
    friend struct ContextChunkProbe;

    GuestPages       *ram_;
    void             *mapping_;
    uint32_t          resource_id_;
    uint32_t          first_page_;
    uint32_t          pages_;
};

}  // namespace av

#endif  // AV_CONTEXT_CHUNK_HPP
