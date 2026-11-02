#include "proto/wire/spill_block.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

SpillBlock::SpillBlock(BlockSource &memory) : memory_(&memory), block_(nullptr), capacity_(0u)
{
    AV_SCOPE("SpillBlock::SpillBlock");
}

SpillBlock::SpillBlock() : memory_(nullptr), block_(nullptr), capacity_(0u)
{
    AV_SCOPE("SpillBlock::SpillBlock");
}

SpillBlock::~SpillBlock()
{
    AV_SCOPE("SpillBlock::~SpillBlock");
    release();
}

uint8_t *SpillBlock::grow(const uint8_t *written, size_t used, size_t wanted, size_t &capacity)
{
    AV_SCOPE("SpillBlock::grow");
    capacity = 0u;
    if (memory_ == nullptr || used > wanted) return nullptr;
    size_t bytes = capacity_ > least_bytes ? capacity_ : least_bytes;
    while (bytes < wanted) {
        if (bytes > ~size_t{0} / 2u) {
            bytes = wanted;
            break;
        }
        bytes *= 2u;
    }
    auto *const grown = static_cast<uint8_t *>(memory_->allocate(bytes, Pool::paged));
    if (grown == nullptr) return nullptr;
    if (used != 0u) __builtin_memcpy(grown, written, used);
    if (block_ != nullptr) memory_->release(block_);
    block_ = grown;
    capacity_ = bytes;
    capacity = bytes;
    return grown;
}

void SpillBlock::release()
{
    AV_SCOPE("SpillBlock::release");
    if (block_ != nullptr) memory_->release(block_);
    block_ = nullptr;
    capacity_ = 0u;
}

uint8_t *SpillBlock::bytes()
{
    AV_SCOPE("SpillBlock::bytes");
    return block_;
}

const uint8_t *SpillBlock::bytes() const
{
    AV_SCOPE("SpillBlock::bytes");
    return block_;
}

size_t SpillBlock::capacity() const
{
    AV_SCOPE("SpillBlock::capacity");
    return capacity_;
}

bool SpillBlock::invariants_hold() const
{
    AV_SCOPE("SpillBlock::invariants_hold");
    return (block_ == nullptr) == (capacity_ == 0u);
}

}
