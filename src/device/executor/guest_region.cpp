#include "device/executor/guest_region.hpp"

#include "core/trace/function_scope.hpp"

#include <sys/mman.h>
#include <unistd.h>

namespace av {

GuestRegion::GuestRegion() : base_(nullptr), length_(0u), descriptor_(-1)
{
    AV_SCOPE("GuestRegion::GuestRegion");
}

void GuestRegion::take(uint8_t *base, uint64_t length)
{
    AV_SCOPE("GuestRegion::take");
    base_ = base;
    length_ = base == nullptr ? 0u : length;
}

bool GuestRegion::take_shared(int descriptor, uint64_t length)
{
    AV_SCOPE("GuestRegion::take_shared");
    if (descriptor < 0 || length == 0u) return false;
    void *const mapped = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, descriptor, 0);
    if (mapped == MAP_FAILED) return false;
    base_ = static_cast<uint8_t *>(mapped);
    length_ = length;
    descriptor_ = descriptor;
    return true;
}

void GuestRegion::give_back()
{
    AV_SCOPE("GuestRegion::give_back");
    if (descriptor_ >= 0) {
        static_cast<void>(munmap(base_, length_));
        static_cast<void>(close(descriptor_));
    }
    base_ = nullptr;
    length_ = 0u;
    descriptor_ = -1;
}

int GuestRegion::descriptor() const
{
    AV_SCOPE("GuestRegion::descriptor");
    return descriptor_;
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
