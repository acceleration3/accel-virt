#include "device/memory/blob_window.hpp"

namespace av {

BlobWindow::BlobWindow() : pointers_(nullptr), lengths_(nullptr), extents_(0u), length_(0u) {}

BlobWindow::BlobWindow(void *const *pointers, const uint64_t *lengths, uint32_t extents, uint64_t length)
    : pointers_(pointers), lengths_(lengths), extents_(extents), length_(length) {}

uint64_t BlobWindow::length() const
{
    return length_;
}

uint32_t BlobWindow::extents() const
{
    return extents_;
}

void *BlobWindow::extent(uint32_t index, uint64_t &bytes) const
{
    bytes = index < extents_ ? lengths_[index] : 0u;
    return index < extents_ ? pointers_[index] : nullptr;
}

bool BlobWindow::contains(uint64_t offset, size_t count) const
{
    return offset <= length_ && count <= length_ - offset;
}

Status BlobWindow::read(uint64_t offset, uint8_t *bytes, size_t count) const
{
    if (!contains(offset, count)) return Status::invalid_parameter;
    walk(offset, count, [bytes](const uint8_t *source, size_t done, size_t chunk) {
        for (size_t i = 0u; i < chunk; i++) bytes[done + i] = source[i];
    });
    return Status::ok;
}

Status BlobWindow::write(uint64_t offset, const uint8_t *bytes, size_t count) const
{
    if (!contains(offset, count)) return Status::invalid_parameter;
    walk(offset, count, [bytes](uint8_t *destination, size_t done, size_t chunk) {
        for (size_t i = 0u; i < chunk; i++) destination[i] = bytes[done + i];
    });
    return Status::ok;
}

/* The extents add up to the length, and there is memory behind any of them. */
bool BlobWindow::invariants_hold() const
{
    if (extents_ != 0u && (pointers_ == nullptr || lengths_ == nullptr)) return false;
    uint64_t sum = 0u;
    for (uint32_t extent = 0u; extent < extents_; extent++) sum += lengths_[extent];
    return sum == length_;
}

}  // namespace av
