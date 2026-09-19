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
 * on this path asks for. */
#ifndef AV_CLICD_CL_ARENA_BLOCKS_HPP
#define AV_CLICD_CL_ARENA_BLOCKS_HPP

#include <stdint.h>

namespace av::clicd {

class ClArenaBlocks {
public:
    /* Buffers one context may have backed by its arena at once. */
    static constexpr uint32_t max_blocks = 64u;
    /* What every block's offset and length are a whole number of. */
    static constexpr uint64_t alignment = 4096u;

    ClArenaBlocks();
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
    /* `bytes` rounded up to alignment, or 0 if that would wrap. */
    uint64_t whole_pages(uint64_t bytes) const;
    /* Where a block of `want` fits, or `bytes_` when none does. Rows are kept
     * in offset order so the gaps are what lies between them. */
    uint64_t gap_for(uint64_t want) const;

    uint8_t *base_;
    uint64_t bytes_;
    uint64_t offsets_[max_blocks];
    uint64_t sizes_[max_blocks];
    uint32_t count_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ARENA_BLOCKS_HPP
