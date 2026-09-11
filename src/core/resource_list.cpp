#include "resource_list.hpp"

#include "invariant_scope.hpp"

namespace av {

namespace {

enum : uint8_t { type_interrupt = 2u, type_memory = 3u, type_memory_large = 7u };

constexpr uint16_t interrupt_message = 0x0002u;  /* CM_RESOURCE_INTERRUPT_MESSAGE */
constexpr uint16_t memory_large_mask = 0x0E00u;
constexpr uint16_t memory_large_40   = 0x0200u;
constexpr uint16_t memory_large_48   = 0x0400u;
constexpr uint16_t memory_large_64   = 0x0800u;

}  // namespace

/* Little-endian reads that stop, and stay stopped, at the first one that would
 * leave the bytes. */
class ResourceList::Cursor {
public:
    Cursor(const uint8_t *bytes, size_t length) : bytes_(bytes), length_(length), at_(0u), failed_(false) {}

    uint64_t read(size_t width)
    {
        if (failed_ || width > length_ - at_) {
            failed_ = true;
            return 0u;
        }
        uint64_t value = 0u;
        for (size_t i = 0u; i < width; i++) value |= uint64_t{bytes_[at_ + i]} << (8u * i);
        at_ += width;
        return value;
    }

    void skip(size_t bytes)
    {
        if (failed_ || bytes > length_ - at_) failed_ = true;
        else at_ += bytes;
    }

    bool failed() const { return failed_; }

private:
    const uint8_t *bytes_;
    size_t         length_;
    size_t         at_;
    bool           failed_;
};

ResourceList::ResourceList(Platform &platform)
    : platform_(platform), memory_starts_(), memory_lengths_(), memory_count_(0u), message_interrupts_(0u) {}

Status ResourceList::add_memory(uint64_t start, uint64_t length)
{
    if (length == 0u || start + length < start) return Status::malformed;
    if (memory_count_ == max_memory_ranges) return Status::too_large;
    memory_starts_[memory_count_] = start;
    memory_lengths_[memory_count_] = length;
    memory_count_++;
    return Status::ok;
}

Status ResourceList::read_partial_descriptor(Cursor &cursor)
{
    const uint8_t type = static_cast<uint8_t>(cursor.read(1u));
    cursor.skip(partial_flags_offset - 1u);
    const uint16_t flags = static_cast<uint16_t>(cursor.read(2u));
    const uint64_t start = cursor.read(8u);
    const uint32_t stored = static_cast<uint32_t>(cursor.read(4u));
    cursor.skip(partial_descriptor_bytes - memory_length_offset - 4u);
    if (cursor.failed()) return Status::malformed;

    if (type == type_interrupt) {
        if ((flags & interrupt_message) != 0u) message_interrupts_++;
        return Status::ok;
    }
    if (type == type_memory) return add_memory(start, stored);
    if (type != type_memory_large) return Status::ok;
    switch (flags & memory_large_mask) {
    case memory_large_40: return add_memory(start, uint64_t{stored} << 8u);
    case memory_large_48: return add_memory(start, uint64_t{stored} << 16u);
    case memory_large_64: return add_memory(start, uint64_t{stored} << 32u);
    default:              return Status::malformed;
    }
}

Status ResourceList::read_full_descriptor(Cursor &cursor, uint32_t &partial_total)
{
    cursor.skip(partial_count_offset);
    const uint64_t count = cursor.read(4u);
    if (cursor.failed()) return Status::malformed;
    if (count > max_partial_descriptors) return Status::too_large;
    for (uint64_t index = 0u; index < count; index++) {
        const Status status = read_partial_descriptor(cursor);
        if (status != Status::ok) return status;
    }
    partial_total += static_cast<uint32_t>(count);
    return Status::ok;
}

Status ResourceList::initialize(const uint8_t *bytes, size_t length)
{
    platform_.require_irql_at_most(Irql::passive, "ResourceList::initialize");
    InvariantScope<ResourceList> guard(platform_, *this, "initialize");
    memory_count_ = 0u;
    message_interrupts_ = 0u;
    Cursor cursor(bytes, bytes == nullptr ? 0u : length);
    const uint64_t full_count = cursor.read(4u);
    if (cursor.failed()) return Status::malformed;
    if (full_count > max_full_descriptors) return Status::too_large;

    Status status = Status::ok;
    uint32_t partial_total = 0u;
    for (uint64_t index = 0u; index < full_count && status == Status::ok; index++)
        status = read_full_descriptor(cursor, partial_total);
    if (status == Status::ok && partial_total == 0u) status = Status::malformed;
    if (status != Status::ok) {
        memory_count_ = 0u;
        message_interrupts_ = 0u;
    }
    return status;
}

Status ResourceList::find_memory(uint64_t start, uint64_t minimum_length, uint64_t &length) const
{
    platform_.require_irql_at_most(Irql::passive, "ResourceList::find_memory");
    for (uint32_t index = 0u; index < memory_count_; index++) {
        if (memory_starts_[index] != start) continue;
        if (memory_lengths_[index] < minimum_length) return Status::malformed;
        length = memory_lengths_[index];
        return Status::ok;
    }
    return Status::not_found;
}

uint32_t ResourceList::message_interrupts() const { return message_interrupts_; }

bool ResourceList::invariants_hold() const
{
    if (memory_count_ > max_memory_ranges) return false;
    for (uint32_t index = 0u; index < memory_count_; index++) {
        const uint64_t start = memory_starts_[index];
        const uint64_t length = memory_lengths_[index];
        if (length == 0u || start + length < start) return false;
    }
    return true;
}

}  // namespace av
