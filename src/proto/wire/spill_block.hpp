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
    SpillBlock();
    ~SpillBlock();
    SpillBlock(const SpillBlock &) = delete;
    SpillBlock &operator=(const SpillBlock &) = delete;

    uint8_t *grow(const uint8_t *written, size_t used, size_t wanted, size_t &capacity) override;
    void     release();
    uint8_t       *bytes();
    const uint8_t *bytes() const;
    size_t         capacity() const;

    bool invariants_hold() const;

private:
    static constexpr size_t least_bytes = 64u * 1024u;

    BlockSource *memory_;
    uint8_t     *block_;
    size_t       capacity_;

    friend struct SpillBlockProbe;
};

}

#endif
