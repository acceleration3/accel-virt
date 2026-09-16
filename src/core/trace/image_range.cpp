#include "core/trace/image_range.hpp"

namespace av {

/* A range whose end would wrap - including one ending exactly at 2^64 - is
 * kept empty: nothing is inside it. */
ImageRange::ImageRange(uint64_t base, uint64_t size) : base_(base), size_(base + size <= base ? 0u : size) {}

bool ImageRange::contains(uint64_t address, uint64_t bytes) const
{
    if (address < base_ || bytes > size_) return false;
    return address - base_ <= size_ - bytes;
}

uint64_t ImageRange::offset_of(uint64_t address) const
{
    return contains(address, 1u) ? address - base_ : address;
}

uint64_t ImageRange::end() const { return base_ + size_; }

bool ImageRange::invariants_hold() const { return size_ == 0u || base_ + size_ > base_; }

}  // namespace av
