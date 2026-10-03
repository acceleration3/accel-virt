/* A block of one context's blob arena that a transfer stages its bytes in for
 * as long as it runs.
 *
 * Bytes in the arena cross by offset (a lent parameter): the host reads a
 * write's straight out of it and writes a read's straight into it, so the
 * application's memory is copied once, here, rather than into a command, out
 * of the ring, into the device's decode arena and back through the reply slot.
 * The block goes back to the arena when the transfer is done with it.
 *
 * What it takes is what the arena can spare: the whole transfer up to `most`,
 * or less, halving, down to the smallest its caller finds worth staging. For
 * a transfer one command carries inline that is `least` - below it the inline
 * command costs no round trip of its own, and the copies are too small to
 * matter. A read into rows with gaps between them is worth any block at all:
 * inline it comes back a row at a time, one round trip each. */
#ifndef AV_CLICD_CL_STAGING_BLOCK_HPP
#define AV_CLICD_CL_STAGING_BLOCK_HPP

#include <stdint.h>

namespace av::clicd {

class ClArenaBlocks;

class ClStagingBlock {
public:
    /* A frame of tiles in one command, and a quarter of the arena: a few
     * transfers at once still leave room for the buffers it backs. */
    static constexpr uint64_t most = uint64_t{4} << 20u;
    static constexpr uint64_t least = uint64_t{64} << 10u;

    explicit ClStagingBlock(ClArenaBlocks &blocks);
    ~ClStagingBlock();
    ClStagingBlock(const ClStagingBlock &) = delete;
    ClStagingBlock &operator=(const ClStagingBlock &) = delete;

    /* Room for `wanted` bytes or as many as the arena has, between
     * `smallest` and `most`; false for a transfer smaller than `smallest`, no
     * arena, no room that large, or a block already taken. */
    [[nodiscard]] bool take(uint64_t wanted, uint64_t smallest);
    uint8_t           *at() const;
    uint64_t           bytes() const;

private:
    ClArenaBlocks &blocks_;
    uint8_t       *at_;
    uint64_t       offset_;
    uint64_t       bytes_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_STAGING_BLOCK_HPP
