#include "proto/wire/wire_reader.hpp"

namespace av {

WireReader::WireReader(const uint8_t *bytes, size_t length)
    : bytes_(bytes), length_(bytes != nullptr ? length : 0u), offset_(0u),
      status_(Status::ok) {}

const uint8_t *WireReader::take(size_t count)
{
    if (status_ != Status::ok || count > length_ - offset_) {
        status_ = Status::malformed;
        return nullptr;
    }
    const uint8_t *start = bytes_ + offset_;
    offset_ += count;
    return start;
}

uint64_t WireReader::read_little_endian(size_t count)
{
    const uint8_t *start = take(count);
    uint64_t value = 0u;
    for (size_t i = 0u; start != nullptr && i < count; i++)
        value |= static_cast<uint64_t>(start[i]) << (8u * i);
    return value;
}

uint8_t  WireReader::read_u8()  { return static_cast<uint8_t>(read_little_endian(1u)); }
uint16_t WireReader::read_u16() { return static_cast<uint16_t>(read_little_endian(2u)); }
uint32_t WireReader::read_u32() { return static_cast<uint32_t>(read_little_endian(4u)); }
uint64_t WireReader::read_u64() { return read_little_endian(8u); }

void WireReader::copy_bytes(uint8_t *destination, size_t count)
{
    /* Builtins, not loops: the freestanding builds do not turn a loop into
     * memcpy, and every byte an upload carries passes through here. */
    const uint8_t *start = take(count);
    if (start != nullptr) __builtin_memcpy(destination, start, count);
    else __builtin_memset(destination, 0, count);
}

void WireReader::expect_zero(size_t count)
{
    const uint8_t *start = take(count);
    for (size_t i = 0u; start != nullptr && i < count; i++) {
        if (start[i] != 0u) status_ = Status::malformed;
    }
}

void WireReader::skip(size_t count) { static_cast<void>(take(count)); }

bool WireReader::read(void *destination, size_t count)
{
    /* A decoder whose arena refused it has nowhere to put the bytes. Reading
     * into nothing is a frame this host cannot take, not a write through null. */
    if (destination == nullptr) {
        status_ = Status::malformed;
        return false;
    }
    copy_bytes(static_cast<uint8_t *>(destination), count);
    return status_ == Status::ok;
}

bool WireReader::read_varint_bits(uint64_t &value, size_t bits)
{
    value = 0u;
    for (size_t shift = 0u; shift < max_varint_bits; shift += 7u) {
        const uint8_t byte = read_u8();
        if (status_ != Status::ok) return false;
        const uint64_t group = static_cast<uint64_t>(byte & 0x7Fu);
        /* A group whose bits do not fit the destination is a frame claiming a
         * value the field cannot hold. */
        if (shift >= bits || (shift + 7u > bits && (group >> (bits - shift)) != 0u)) {
            status_ = Status::malformed;
            return false;
        }
        value |= group << shift;
        if ((byte & 0x80u) == 0u) return true;
    }
    status_ = Status::malformed;
    return false;
}

Status WireReader::status() const    { return status_; }
size_t WireReader::offset() const    { return offset_; }
/* Zero after a failure, so a loop reading until nothing remains always ends. */
size_t WireReader::remaining() const { return status_ == Status::ok ? length_ - offset_ : 0u; }

bool WireReader::invariants_hold() const
{
    return offset_ <= length_ && (bytes_ != nullptr || length_ == 0u);
}

}  // namespace av
