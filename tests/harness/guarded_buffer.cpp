#include "guarded_buffer.hpp"

#include <cstdlib>
#include <cstring>

namespace av {

GuardedBuffer::GuardedBuffer(std::size_t size)
    : raw_(static_cast<std::uint8_t *>(std::malloc(size + 2u * guard_bytes))),
      size_(size)
{
    std::memset(raw_, poison, size + 2u * guard_bytes);
    std::memset(raw_ + guard_bytes, 0, size);
}

GuardedBuffer::~GuardedBuffer() { std::free(raw_); }

std::uint8_t *GuardedBuffer::data() { return raw_ + guard_bytes; }
std::size_t   GuardedBuffer::size() const { return size_; }

bool GuardedBuffer::guards_intact() const
{
    for (std::size_t i = 0u; i < guard_bytes; i++) {
        if (raw_[i] != poison) return false;
        if (raw_[guard_bytes + size_ + i] != poison) return false;
    }
    return true;
}

}  // namespace av
