/* One chunk of host GPU memory an av context's process holds
 * (docs/host-memory.md): the host blob the device made for the context, and
 * either the run of window pages it is placed at with the platform's record of
 * the process's view of them, or - for pages the process lent ("Application
 * memory") - the platform's lock on those pages. What taking it down needs. */
#ifndef AV_CONTEXT_CHUNK_HPP
#define AV_CONTEXT_CHUNK_HPP

#include <stdint.h>

namespace av {

class ContextChunk {
public:
    /* Context slot `slot`'s chunk: blob `resource_id`, never 0, placed at
     * `pages` window pages from `first_page`, viewed through `mapping`. */
    ContextChunk(uint32_t slot, uint32_t resource_id, uint32_t first_page, uint32_t pages, void *mapping);
    /* Context slot `slot`'s blob `resource_id` over the process pages
     * `locked` holds: no window pages and no view. */
    ContextChunk(uint32_t slot, uint32_t resource_id, void *locked);
    ContextChunk(const ContextChunk &) = delete;
    ContextChunk &operator=(const ContextChunk &) = delete;

    uint32_t slot() const;
    uint32_t resource_id() const;
    uint32_t first_page() const;
    uint32_t pages() const;
    void    *mapping() const;
    /* The lock on the process's pages; null for a window chunk. */
    void    *locked() const;

    /* A resource, and either a lock on process pages or at least a window
     * page with a view of it. */
    bool invariants_hold() const;

private:
    uint32_t slot_;
    uint32_t resource_id_;
    uint32_t first_page_;
    uint32_t pages_;
    void    *mapping_;
    void    *locked_;
};

}  // namespace av

#endif  // AV_CONTEXT_CHUNK_HPP
