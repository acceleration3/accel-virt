#include "device/memory/blob_window.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

BlobWindow::BlobWindow() : pointers_(nullptr), lengths_(nullptr), extents_(0u), length_(0u)
{
    AV_SCOPE("BlobWindow::BlobWindow");
}

BlobWindow::BlobWindow(void *const *pointers, const uint64_t *lengths, uint32_t extents, uint64_t length)
    : pointers_(pointers), lengths_(lengths), extents_(extents), length_(length)
    {
        AV_SCOPE("BlobWindow::BlobWindow");
    }

uint64_t BlobWindow::length() const
{
    AV_SCOPE("BlobWindow::length");
    return length_;
}

uint32_t BlobWindow::extents() const
{
    AV_SCOPE("BlobWindow::extents");
    return extents_;
}

void *BlobWindow::extent(uint32_t index, uint64_t &bytes) const
{
    AV_SCOPE("BlobWindow::extent");
    bytes = index < extents_ ? lengths_[index] : 0u;
    return index < extents_ ? pointers_[index] : nullptr;
}

bool BlobWindow::contains(uint64_t offset, size_t count) const
{
    AV_SCOPE("BlobWindow::contains");
    return offset <= length_ && count <= length_ - offset;
}

Status BlobWindow::read(uint64_t offset, uint8_t *bytes, size_t count) const
{
    AV_SCOPE("BlobWindow::read");
    if (!contains(offset, count)) return Status::invalid_parameter;
    walk(offset, count, [bytes](const uint8_t *source, size_t done, size_t chunk) {
        __builtin_memcpy(bytes + done, source, chunk);
    });
    return Status::ok;
}

Status BlobWindow::write(uint64_t offset, const uint8_t *bytes, size_t count) const
{
    AV_SCOPE("BlobWindow::write");
    if (!contains(offset, count)) return Status::invalid_parameter;
    walk(offset, count, [bytes](uint8_t *destination, size_t done, size_t chunk) {
        __builtin_memcpy(destination, bytes + done, chunk);
    });
    return Status::ok;
}

uint64_t *BlobWindow::word(uint64_t offset) const
{
    AV_SCOPE("BlobWindow::word");
    if (!contains(offset, sizeof(uint64_t))) return nullptr;
    uint64_t *found = nullptr;
    walk(offset, sizeof(uint64_t), [&found](uint8_t *at, size_t, size_t chunk) {
        if (chunk == sizeof(uint64_t) && reinterpret_cast<uintptr_t>(at) % alignof(uint64_t) == 0u)
            found = reinterpret_cast<uint64_t *>(at);
    });
    return found;
}

Status BlobWindow::load_u64(uint64_t offset, uint64_t &value) const
{
    AV_SCOPE("BlobWindow::load_u64");
    const uint64_t *const field = word(offset);
    if (field == nullptr) return Status::invalid_parameter;
    value = __atomic_load_n(field, __ATOMIC_ACQUIRE);
    return Status::ok;
}

Status BlobWindow::store_u64(uint64_t offset, uint64_t value) const
{
    AV_SCOPE("BlobWindow::store_u64");
    uint64_t *const field = word(offset);
    if (field == nullptr) return Status::invalid_parameter;
    __atomic_store_n(field, value, __ATOMIC_RELEASE);
    return Status::ok;
}

bool BlobWindow::invariants_hold() const
{
    AV_SCOPE("BlobWindow::invariants_hold");
    if (extents_ != 0u && (pointers_ == nullptr || lengths_ == nullptr)) return false;
    uint64_t sum = 0u;
    for (uint32_t extent = 0u; extent < extents_; extent++) sum += lengths_[extent];
    return sum == length_;
}

}
