#include "proto/wire/wire_writer.hpp"

namespace av {

WireWriter::WireWriter(uint8_t *bytes, size_t capacity)
    : bytes_(bytes), capacity_(bytes != nullptr ? capacity : 0u), offset_(0u), wanted_(0u),
      status_(Status::ok) {}

uint8_t *WireWriter::claim(size_t count)
{
    /* Counted before the room is checked, and counted whether or not this one
     * fits: the answer's length is the answer's, not the prefix that fitted. */
    wanted_ += count;
    if (status_ != Status::ok || count > capacity_ - offset_) {
        status_ = Status::too_large;
        return nullptr;
    }
    uint8_t *start = bytes_ + offset_;
    offset_ += count;
    return start;
}

void WireWriter::write_little_endian(uint64_t value, size_t count)
{
    uint8_t *start = claim(count);
    for (size_t i = 0u; start != nullptr && i < count; i++)
        start[i] = static_cast<uint8_t>(value >> (8u * i));
}

void WireWriter::write_u8(uint8_t value)   { write_little_endian(value, 1u); }
void WireWriter::write_u16(uint16_t value) { write_little_endian(value, 2u); }
void WireWriter::write_u32(uint32_t value) { write_little_endian(value, 4u); }
void WireWriter::write_u64(uint64_t value) { write_little_endian(value, 8u); }

void WireWriter::copy_bytes(const uint8_t *source, size_t count)
{
    uint8_t *start = claim(count);
    for (size_t i = 0u; start != nullptr && i < count; i++) start[i] = source[i];
}

void WireWriter::write_zeros(size_t count)
{
    uint8_t *start = claim(count);
    for (size_t i = 0u; start != nullptr && i < count; i++) start[i] = 0u;
}

void WireWriter::write(const void *source, size_t count)
{
    copy_bytes(static_cast<const uint8_t *>(source), count);
}

/* Written into a scratch of its own first, so a varint that does not fit
 * leaves nothing behind - what every other write here promises. */
void WireWriter::write_varint_bits(uint64_t value)
{
    uint8_t groups[10] = {};
    size_t  count = 0u;
    do {
        uint8_t group = static_cast<uint8_t>(value & 0x7Fu);
        value >>= 7u;
        if (value != 0u) group |= 0x80u;
        groups[count++] = group;
    } while (value != 0u);
    copy_bytes(groups, count);
}

Status WireWriter::status() const { return status_; }
size_t WireWriter::offset() const { return offset_; }
size_t WireWriter::wanted() const { return wanted_; }

bool WireWriter::invariants_hold() const
{
    return offset_ <= capacity_ && offset_ <= wanted_ && (bytes_ != nullptr || capacity_ == 0u);
}

}  // namespace av
