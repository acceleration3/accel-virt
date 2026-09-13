#include "proto/wire/wire_arena.hpp"

namespace av {

WireArena::WireArena(uint8_t *block, size_t capacity)
    : block_(block), capacity_(block == nullptr ? 0u : capacity), used_(0u) {}

void *WireArena::claim(size_t count, size_t size, size_t alignment)
{
    if (count == 0u || size == 0u) return nullptr;
    /* The count arrives from the wire: refuse what would wrap rather than
     * hand back room for less than was asked for. */
    if (count > ~size_t{0} / size) return nullptr;
    const size_t bytes = count * size;
    const size_t padding = (alignment - (used_ % alignment)) % alignment;
    if (padding > capacity_ - used_) return nullptr;
    const size_t start = used_ + padding;
    if (bytes > capacity_ - start) return nullptr;
    used_ = start + bytes;
    return block_ + start;
}

char *WireArena::allocate_chars(size_t count)
{
    return static_cast<char *>(claim(count, sizeof(char), alignof(char)));
}

void   WireArena::reset() { used_ = 0u; }
size_t WireArena::used() const { return used_; }
size_t WireArena::capacity() const { return capacity_; }

bool WireArena::invariants_hold() const
{
    return used_ <= capacity_ && (block_ != nullptr || capacity_ == 0u);
}

}  // namespace av
