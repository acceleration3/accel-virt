#include "proto/wire/guest_bytes.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestBytes::GuestBytes() : base_(nullptr), length_(0u)
{
    AV_SCOPE("GuestBytes::GuestBytes");
}

GuestBytes::GuestBytes(uint8_t *base, uint64_t length)
    : base_(base != nullptr && length != 0u ? base : nullptr), length_(base != nullptr ? length : 0u)
{
    AV_SCOPE("GuestBytes::GuestBytes");
}

bool GuestBytes::bind(uint8_t *base, uint64_t length)
{
    AV_SCOPE("GuestBytes::bind");
    if (base_ != nullptr || base == nullptr || length == 0u) return false;
    base_ = base;
    length_ = length;
    return true;
}

void GuestBytes::unbind()
{
    AV_SCOPE("GuestBytes::unbind");
    base_ = nullptr;
    length_ = 0u;
}

bool GuestBytes::over(const uint8_t *base) const
{
    AV_SCOPE("GuestBytes::over");
    return base_ != nullptr && base_ == base;
}

uint8_t *GuestBytes::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("GuestBytes::at");
    /* Subtraction rather than `offset + bytes`, which is what a guest would
     * pick the numbers to wrap. */
    if (base_ == nullptr || offset > length_ || bytes > length_ - offset) return nullptr;
    /* Within length_, which a mapping of this process holds, so it fits. */
    return base_ + static_cast<uintptr_t>(offset);
}

bool GuestBytes::holds(const void *bytes) const
{
    AV_SCOPE("GuestBytes::holds");
    /* As integers: the pointer may be into any other object. One below the
     * base wraps to an offset past the end. */
    return base_ != nullptr && reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(base_) < length_;
}

uint64_t GuestBytes::length() const
{
    AV_SCOPE("GuestBytes::length");
    return length_;
}

bool GuestBytes::invariants_hold() const
{
    AV_SCOPE("GuestBytes::invariants_hold");
    return (base_ == nullptr) == (length_ == 0u);
}

}  // namespace av
