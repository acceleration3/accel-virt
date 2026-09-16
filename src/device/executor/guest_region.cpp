#include "device/executor/guest_region.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestRegion::GuestRegion() : base_(nullptr), length_(0u)
{
    AV_SCOPE("GuestRegion::GuestRegion");
}

void GuestRegion::take(uint8_t *base, uint64_t length)
{
    AV_SCOPE("GuestRegion::take");
    base_ = base;
    length_ = base == nullptr ? 0u : length;
}

void GuestRegion::give_back()
{
    AV_SCOPE("GuestRegion::give_back");
    base_ = nullptr;
    length_ = 0u;
}

uint8_t *GuestRegion::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("GuestRegion::at");
    if (base_ == nullptr) return nullptr;
    if (offset > length_) return nullptr;
    /* Subtraction rather than `offset + bytes`, which is what a guest would
     * pick the numbers to wrap. */
    if (bytes > length_ - offset) return nullptr;
    return base_ + offset;
}

uint64_t GuestRegion::length() const
{
    AV_SCOPE("GuestRegion::length");
    return length_;
}

bool GuestRegion::invariants_hold() const
{
    AV_SCOPE("GuestRegion::invariants_hold");
    return base_ != nullptr || length_ == 0u;
}

}  // namespace av
