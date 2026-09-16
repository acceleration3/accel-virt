#include "core/support/byte_range.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ByteRange::ByteRange() : base_(0u), size_(0u)
{
    AV_SCOPE("ByteRange::ByteRange");
}

ByteRange::ByteRange(uint64_t base, uint64_t size)
    : base_(base), size_(size)
    {
        AV_SCOPE("ByteRange::ByteRange");
    }

bool ByteRange::is_representable() const
{
    AV_SCOPE("ByteRange::is_representable");
    return size_ != 0u && base_ <= UINT64_MAX - size_;
}

uint64_t ByteRange::base() const
{
    AV_SCOPE("ByteRange::base");
    return base_;
}
uint64_t ByteRange::size() const
{
    AV_SCOPE("ByteRange::size");
    return size_;
}
uint64_t ByteRange::end() const
{
    AV_SCOPE("ByteRange::end");
    return base_ + size_;
}

bool ByteRange::contains(uint64_t address) const
{
    AV_SCOPE("ByteRange::contains");
    return address >= base_ && address - base_ < size_;
}

bool ByteRange::encloses(const ByteRange &inner) const
{
    AV_SCOPE("ByteRange::encloses");
    if (!is_representable() || !inner.is_representable()) return false;
    return inner.base_ >= base_ && inner.end() <= end();
}

bool ByteRange::starts_below(const ByteRange &other) const
{
    AV_SCOPE("ByteRange::starts_below");
    return base_ < other.base_;
}

}  // namespace av
