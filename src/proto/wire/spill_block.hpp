/* A WireSpill over memory of its own: one block from a BlockSource, doubled
 * as a writer outgrows it and kept until released. What a transport lends a
 * command longer than its room into, and what the executor keeps an answer
 * longer than the reply slot in (docs/protocol.md, "Long commands"). */
#ifndef AV_SPILL_BLOCK_HPP
#define AV_SPILL_BLOCK_HPP

#include "proto/wire/wire_spill.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
struct SpillBlockProbe;

class SpillBlock final : public WireSpill {
public:
    explicit SpillBlock(BlockSource &memory);
    /* A spill with no memory, which grows nothing: what a transport whose
     * owner bounds every command it sends is lent (KmtTransport). */
    SpillBlock();
    ~SpillBlock();
    SpillBlock(const SpillBlock &) = delete;
    SpillBlock &operator=(const SpillBlock &) = delete;

    uint8_t *grow(const uint8_t *written, size_t used, size_t wanted, size_t &capacity) override;
    /* The block back to its source; a writer that spilled into it writes
     * nowhere afterwards. */
    void     release();
    /* The block, null before a grow and after release. */
    uint8_t       *bytes();
    const uint8_t *bytes() const;
    size_t         capacity() const;

    bool invariants_hold() const;

private:
    /* The least a block is: a spill is for what outgrew a room, so a
     * smaller one would only be grown again. */
    static constexpr size_t least_bytes = 64u * 1024u;

    BlockSource *memory_;
    uint8_t     *block_;
    size_t       capacity_;

    friend struct SpillBlockProbe;
};

}  // namespace av

#endif  // AV_SPILL_BLOCK_HPP
