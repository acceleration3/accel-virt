#include "wire_reader.hpp"

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
    const uint8_t *start = take(count);
    for (size_t i = 0u; i < count; i++)
        destination[i] = (start != nullptr) ? start[i] : 0u;
}

void WireReader::expect_zero(size_t count)
{
    const uint8_t *start = take(count);
    for (size_t i = 0u; start != nullptr && i < count; i++) {
        if (start[i] != 0u) status_ = Status::malformed;
    }
}

Status WireReader::status() const    { return status_; }
size_t WireReader::offset() const    { return offset_; }
size_t WireReader::remaining() const { return length_ - offset_; }

bool WireReader::invariants_hold() const
{
    return offset_ <= length_ && (bytes_ != nullptr || length_ == 0u);
}

}  // namespace av
