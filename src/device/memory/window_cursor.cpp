#include "device/memory/window_cursor.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WindowCursor::WindowCursor(const BlobWindow &window) : window_(window), extent_(0u), start_(0u)
{
    AV_SCOPE("WindowCursor::WindowCursor");
}

Status WindowCursor::read(uint64_t offset, uint8_t *bytes, size_t count)
{
    AV_SCOPE("WindowCursor::read");
    if (!contains(offset, count)) return Status::invalid_parameter;
    for (size_t done = 0u; done < count;) {
        uint64_t             available = 0u;
        const uint8_t *const from = at(offset + done, available);
        if (from == nullptr) return Status::invalid_parameter;
        const size_t         piece = available < count - done ? static_cast<size_t>(available) : count - done;
        __builtin_memcpy(bytes + done, from, piece);
        done += piece;
    }
    return Status::ok;
}

Status WindowCursor::write(uint64_t offset, const uint8_t *bytes, size_t count)
{
    AV_SCOPE("WindowCursor::write");
    if (!contains(offset, count)) return Status::invalid_parameter;
    for (size_t done = 0u; done < count;) {
        uint64_t       available = 0u;
        uint8_t *const to = at(offset + done, available);
        if (to == nullptr) return Status::invalid_parameter;
        const size_t   piece = available < count - done ? static_cast<size_t>(available) : count - done;
        __builtin_memcpy(to, bytes + done, piece);
        done += piece;
    }
    return Status::ok;
}

Status WindowCursor::copy_to(WindowCursor &destination, uint64_t from, uint64_t to, uint64_t count)
{
    AV_SCOPE("WindowCursor::copy_to");
    if (!contains(from, count) || !destination.contains(to, count)) return Status::invalid_parameter;
    for (uint64_t done = 0u; done < count;) {
        uint64_t             readable = 0u, writable = 0u;
        const uint8_t *const source = at(from + done, readable);
        uint8_t *const       target = destination.at(to + done, writable);
        if (source == nullptr || target == nullptr) return Status::invalid_parameter;
        uint64_t piece = readable < writable ? readable : writable;
        if (piece > count - done) piece = count - done;
        __builtin_memmove(target, source, static_cast<size_t>(piece));
        done += piece;
    }
    return Status::ok;
}

uint8_t *WindowCursor::at(uint64_t offset, uint64_t &bytes)
{
    AV_SCOPE("WindowCursor::at");
    if (offset < start_) {
        extent_ = 0u;
        start_ = 0u;
    }
    for (;;) {
        uint64_t    length = 0u;
        void *const base = window_.extent(extent_, length);
        if (base == nullptr) {
            bytes = 0u;
            return nullptr;
        }
        if (offset - start_ < length) {
            bytes = length - (offset - start_);
            return static_cast<uint8_t *>(base) + (offset - start_);
        }
        start_ += length;
        extent_++;
    }
}

bool WindowCursor::contains(uint64_t offset, size_t count) const
{
    AV_SCOPE("WindowCursor::contains");
    return offset <= window_.length() && count <= window_.length() - offset;
}

}  // namespace av
