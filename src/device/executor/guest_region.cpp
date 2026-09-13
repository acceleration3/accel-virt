#include "device/executor/guest_region.hpp"

namespace av {

GuestRegion::GuestRegion() : base_(nullptr), length_(0u) {}

void GuestRegion::take(uint8_t *base, uint64_t length)
{
    base_ = base;
    length_ = base == nullptr ? 0u : length;
}

void GuestRegion::give_back()
{
    base_ = nullptr;
    length_ = 0u;
}

uint8_t *GuestRegion::at(uint64_t offset, uint64_t bytes) const
{
    if (base_ == nullptr) return nullptr;
    if (offset > length_) return nullptr;
    /* Subtraction rather than `offset + bytes`, which is what a guest would
     * pick the numbers to wrap. */
    if (bytes > length_ - offset) return nullptr;
    return base_ + offset;
}

uint64_t GuestRegion::length() const { return length_; }

bool GuestRegion::invariants_hold() const { return base_ != nullptr || length_ == 0u; }

}  // namespace av
