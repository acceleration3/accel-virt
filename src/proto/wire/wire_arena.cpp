#include "proto/wire/wire_arena.hpp"

namespace av {

WireArena::WireArena(uint8_t *block, size_t capacity)
    : block_(block), capacity_(block == nullptr ? 0u : capacity), used_(0u), refusals_(0u) {}

void *WireArena::claim(size_t count, size_t size, size_t alignment)
{
    if (count == 0u) return refuse();
    /* The count arrives from the wire: refuse what would wrap rather than
     * hand back room for less than was asked for. */
    if (count > ~size_t{0} / size) return refuse();
    const size_t bytes = count * size;
    const size_t padding = (alignment - (used_ % alignment)) % alignment;
    if (padding > capacity_ - used_) return refuse();
    const size_t start = used_ + padding;
    if (bytes > capacity_ - start) return refuse();
    used_ = start + bytes;
    /* Zeroed, because a decoder reads some of what it is about to write: the
     * generated pNext codec walks the destination's existing chain to decide
     * whether to fill a node in place or adopt a new one, and a nested struct
     * decoded into fresh arena memory would have walked whatever was there.
     * Found by tests/fuzz/fuzz_generated_decode.cpp on its first run. */
    for (size_t i = 0u; i < bytes; i++)
        block_[start + i] = 0u;
    return block_ + start;
}

char *WireArena::allocate_chars(size_t count)
{
    return static_cast<char *>(claim(count, sizeof(char), alignof(char)));
}

void WireArena::attach(uint8_t *block, size_t capacity)
{
    block_ = block;
    capacity_ = block == nullptr ? 0u : capacity;
    used_ = 0u;
}

void *WireArena::refuse()
{
    refusals_++;
    return nullptr;
}

/* Refusals survive it: they count what this block was too small for, not what
 * one decode was. */
void   WireArena::reset() { used_ = 0u; }
size_t WireArena::used() const { return used_; }
size_t WireArena::capacity() const { return capacity_; }
size_t WireArena::refusals() const { return refusals_; }

bool WireArena::invariants_hold() const
{
    /* That a block-less arena has no capacity is the constructor's doing and
     * cannot come apart afterwards - neither the block nor the capacity is ever
     * assigned again - so what is left to hold is where the mark sits. */
    return used_ <= capacity_;
}

}  // namespace av
