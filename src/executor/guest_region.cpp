#include "executor/guest_region.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "executor/imported_blob.hpp"

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

/* mmap places a mapping at page alignment only, which left a 1 GiB arena where
 * no heap in it could be imported. Address space with room to align is
 * reserved first, the dma-buf mapped over its aligned part, and the rest of
 * the reservation - either side of which may be empty, which munmap refuses
 * and changes nothing for - given back. */
bool GuestRegion::take_shared(int descriptor, uint64_t length)
{
    AV_SCOPE("GuestRegion::take_shared");
    if (descriptor < 0 || length == 0u) return false;
    const uint64_t page = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    const uint64_t mapped_bytes = (length + page - 1u) / page * page;
    const uint64_t reserved_bytes = mapped_bytes + base_alignment;
    void *const reserved = mmap(nullptr, reserved_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (reserved == MAP_FAILED) return false;
    const uintptr_t start = reinterpret_cast<uintptr_t>(reserved);
    const uintptr_t aligned = (start + base_alignment - 1u) & ~uintptr_t{base_alignment - 1u};
    void *const mapped = mmap(reinterpret_cast<void *>(aligned), length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
                              descriptor, 0);
    if (mapped == MAP_FAILED) {
        static_cast<void>(munmap(reserved, reserved_bytes));
        return false;
    }
    static_cast<void>(munmap(reserved, aligned - start));
    static_cast<void>(munmap(reinterpret_cast<void *>(aligned + mapped_bytes), start + reserved_bytes - aligned - mapped_bytes));
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

bool GuestRegion::holds(const void *bytes) const
{
    AV_SCOPE("GuestRegion::holds");
    if (base_ == nullptr) return false;
    /* As integers: the pointer may be into any other object. One below the
     * region wraps to an offset past it. */
    return reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(base_) < length_;
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

bool GuestRegion::adopt_pointer(ImportedBlob &blob)
{
    AV_SCOPE("GuestRegion::adopt_pointer");
    if (length_ != 0u || blob.kind() != AV_BLOB_HOST_POINTER) return false;
    take(static_cast<uint8_t *>(blob.pointer()), blob.size());
    record(blob);
    return true;
}

bool GuestRegion::adopt_pointer_or_buffer(ImportedBlob &blob)
{
    AV_SCOPE("GuestRegion::adopt_pointer_or_buffer");
    if (length_ != 0u || blob.kind() != AV_BLOB_DMA_BUF) return adopt_pointer(blob);
    if (!take_shared(blob.descriptor(), blob.size())) return false;
    record(blob);
    return true;
}

void GuestRegion::record(ImportedBlob &blob) const
{
    AV_SCOPE("GuestRegion::record");
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(base_));
    blob.record(address, address);
}

void GuestRegion::abandon(ImportedBlob &blob)
{
    AV_SCOPE("GuestRegion::abandon");
    if (blob.object() == 0u) return;
    give_back();
    blob.record(0u, 0u);
}

bool GuestRegion::publish(uint64_t offset, uint64_t value)
{
    AV_SCOPE("GuestRegion::publish");
    uint8_t *const word = at(offset, sizeof(uint64_t));
    if (word == nullptr || reinterpret_cast<uintptr_t>(word) % sizeof(uint64_t) != 0u) return false;
    __atomic_store_n(reinterpret_cast<uint64_t *>(word), value, __ATOMIC_RELEASE);
    return true;
}

}  // namespace av
