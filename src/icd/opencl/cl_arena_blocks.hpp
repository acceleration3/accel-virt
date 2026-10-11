/* The blocks one context's buffers are carved out of its blob arena.
 *
 * A buffer backed here costs no copy to map: the device makes the host's
 * cl_mem over this same memory with CL_MEM_USE_HOST_PTR, so the guest's view
 * and the host's are one allocation. That is only true while the block lasts
 * as long as the buffer, which is why this is a suballocator with a free list
 * and not the bump arena the generated clients lease from and retire
 * together - those leases are read once, at the command's send.
 *
 * Blocks are page aligned because a host asked to take memory as a buffer's
 * own wants its device's base alignment, and a page is the coarsest anything
 * on this path asks for.
 *
 * A transfer stages its bytes in a block for as long as it runs
 * (ClStagingBlock), so blocks come and go on every application thread at once:
 * each call holds the lock it is given, which is nothing else's. */
#ifndef AV_CLICD_CL_ARENA_BLOCKS_HPP
#define AV_CLICD_CL_ARENA_BLOCKS_HPP

#include "icd/common/range_table.hpp"

#include <stdint.h>

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::clicd {

struct ClArenaBlocksProbe;

class ClArenaBlocks {
public:
    /* Buffers one context may have backed by its arena at once. Affinity
     * keeps hundreds of small host-accessible buffers alive and maps each
     * every frame; past this each is staged, and its map and unmap are a
     * read and a write over the wire. */
    static constexpr uint32_t max_blocks = 1024u;
    /* What every block's offset and length are a whole number of. */
    static constexpr uint64_t alignment = 4096u;

    explicit ClArenaBlocks(icd::ThreadLock &lock);
    ClArenaBlocks(const ClArenaBlocks &) = delete;
    ClArenaBlocks &operator=(const ClArenaBlocks &) = delete;

    /* Carves from `bytes` at `base` from now on, forgetting every block of
     * whatever was lent before. A null base is no arena at all. */
    void lend(uint8_t *base, uint64_t bytes);
    bool ready() const;

    /* An offset with room for `bytes`, or false with `offset` untouched: for
     * no arena, a block of nothing, a table with no row left, and an arena
     * with no gap big enough. The caller stages that buffer instead. */
    [[nodiscard]] bool take(uint64_t bytes, uint64_t &offset);
    /* Forgets the block at `offset`. An offset no block holds is ignored. */
    void               give_back(uint64_t offset);

    /* The guest's view of `bytes` at `offset`, or null unless one live block
     * begins there and is long enough. */
    uint8_t *at(uint64_t offset, uint64_t bytes) const;

    uint32_t live() const;

    bool invariants_hold() const;

private:
    friend struct ClArenaBlocksProbe;

    icd::ThreadLock                 &lock_;
    uint8_t                         *base_;
    uint64_t                         bytes_;
    /* The blocks, first fit over the arena (RangeTable). */
    icd::RangeTable<max_blocks>      blocks_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ARENA_BLOCKS_HPP
