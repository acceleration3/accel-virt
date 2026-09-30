/* One host-visible chunk of a connection, and the page ranges placed in it
 * (research.md §7, contracts/host_visible_memory.md §1). The chunk is the
 * kernel's resource of the connection's context, mapped into this process: a
 * chunk of the device's host window, or guest RAM when the window has none to
 * give (docs/host-window.md); each range placed in it is imported separately as device memory,
 * so the ranges a chunk holds are bounded as a context's imports are. Ranges
 * are whole pages, placed first fit and kept in address order. */
#ifndef AV_ICD_CHUNK_SPACE_HPP
#define AV_ICD_CHUNK_SPACE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/host_window_chunk.hpp"

#include <stdint.h>

namespace av::icd {

class Connection;
struct ChunkSpaceProbe;

class ChunkSpace {
public:
    static constexpr uint64_t page_bytes = 4096u;
    static constexpr uint32_t max_ranges = 256u;

    explicit ChunkSpace(Connection &connection);
    /* Gives the chunk back whatever is still placed: its device is gone. */
    ~ChunkSpace();
    ChunkSpace(const ChunkSpace &) = delete;
    ChunkSpace &operator=(const ChunkSpace &) = delete;

    /* A chunk of `pages` bound through the connection. invalid_state when
     * open; invalid_parameter for no pages or more than 64 bits of bytes, not
     * asked; malformed when the kernel bound fewer; otherwise the
     * connection's refusal. Nothing is held unless ok. */
    [[nodiscard]] Status open(uint64_t pages);
    /* `pages` placed first fit; `offset` is its first byte. invalid_state when
     * not open, invalid_parameter for no pages, no_memory with no hole that
     * large or every range in use; `offset` is left unless ok. */
    [[nodiscard]] Status place(uint64_t pages, uint64_t &offset);
    /* The range beginning at byte `offset`; invalid_parameter for any other. */
    [[nodiscard]] Status remove(uint64_t offset);
    /* Gives the chunk back: ok with nothing done when not open, invalid_state
     * while anything is placed, else the release's first refusal. */
    [[nodiscard]] Status close();

    bool                    opened() const;
    bool                    empty() const;
    uint32_t                placed() const;
    /* The chunk's pages and the device resource it was bound as; 0 closed. */
    uint64_t                pages() const;
    uint32_t                resource_id() const;
    /* This process's view of the chunk's first byte; null closed. */
    volatile unsigned char *view() const;

    /* Nothing placed or sized while closed; while open, ranges of at least a
     * page, in address order, apart, and inside the chunk. */
    bool invariants_hold() const;

private:
    /* The index the range at `first` would take: the first one after it. */
    uint32_t hole_before(uint64_t pages, uint64_t &first) const;

    Connection                     &connection_;
    ObjectStorage<HostWindowChunk>        chunk_;
    uint64_t                        pages_;
    uint32_t                        placed_;
    /* The placed ranges in address order: first page and page count. */
    uint64_t                        firsts_[max_ranges];
    uint64_t                        lengths_[max_ranges];

    friend struct ChunkSpaceProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CHUNK_SPACE_HPP
