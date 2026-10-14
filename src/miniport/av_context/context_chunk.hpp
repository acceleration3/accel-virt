/* One chunk of the host window an av context's process holds
 * (docs/host-memory.md): the host blob of host GPU memory the device made for
 * the context, the run of window pages it is placed at, and the platform's
 * record of the process's view of them - what taking it down needs. */
#ifndef AV_CONTEXT_CHUNK_HPP
#define AV_CONTEXT_CHUNK_HPP

#include <stdint.h>

namespace av {

class ContextChunk {
public:
    /* Context slot `slot`'s chunk: blob `resource_id`, never 0, placed at
     * `pages` window pages from `first_page`, viewed through `mapping`. */
    ContextChunk(uint32_t slot, uint32_t resource_id, uint32_t first_page, uint32_t pages, void *mapping);
    ContextChunk(const ContextChunk &) = delete;
    ContextChunk &operator=(const ContextChunk &) = delete;

    uint32_t slot() const;
    uint32_t resource_id() const;
    uint32_t first_page() const;
    uint32_t pages() const;
    void    *mapping() const;

    /* A resource, at least a page, and a view of it. */
    bool invariants_hold() const;

private:
    uint32_t slot_;
    uint32_t resource_id_;
    uint32_t first_page_;
    uint32_t pages_;
    void    *mapping_;
};

}  // namespace av

#endif  // AV_CONTEXT_CHUNK_HPP
