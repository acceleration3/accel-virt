#include "core/support/byte_range.hpp"

namespace av {

ByteRange::ByteRange() : base_(0u), size_(0u) {}

ByteRange::ByteRange(uint64_t base, uint64_t size)
    : base_(base), size_(size) {}

bool ByteRange::is_representable() const
{
    return size_ != 0u && base_ <= UINT64_MAX - size_;
}

uint64_t ByteRange::base() const { return base_; }
uint64_t ByteRange::size() const { return size_; }
uint64_t ByteRange::end() const  { return base_ + size_; }

bool ByteRange::contains(uint64_t address) const
{
    return address >= base_ && address - base_ < size_;
}

bool ByteRange::encloses(const ByteRange &inner) const
{
    if (!is_representable() || !inner.is_representable()) return false;
    return inner.base_ >= base_ && inner.end() <= end();
}

bool ByteRange::starts_below(const ByteRange &other) const
{
    return base_ < other.base_;
}

}  // namespace av
