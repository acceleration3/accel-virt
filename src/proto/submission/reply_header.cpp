#include "proto/submission/reply_header.hpp"

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ReplyHeader::ReplyHeader() : status_(status_ok), fence_id_(0u), size_(0u), required_(0u) {}

ReplyHeader::ReplyHeader(uint64_t fence_id, uint32_t size)
    : status_(status_ok), fence_id_(fence_id), size_(size), required_(size) {}

ReplyHeader::ReplyHeader(uint32_t status, uint64_t fence_id, uint32_t size, uint32_t required)
    : status_(status), fence_id_(fence_id), size_(size), required_(required) {}

ReplyHeader ReplyHeader::within(uint32_t capacity) const
{
    return required_ <= capacity ? *this : ReplyHeader(status_too_large, fence_id_, 0u, required_);
}

bool     ReplyHeader::too_large() const { return status_ == status_too_large; }
uint64_t ReplyHeader::fence_id() const  { return fence_id_; }
uint32_t ReplyHeader::size() const      { return size_; }
uint32_t ReplyHeader::required() const  { return required_; }

Status ReplyHeader::encode(uint8_t *bytes, size_t capacity) const
{
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(status_);
    writer.write_u64(fence_id_);
    writer.write_u32(size_);
    writer.write_u32(required_);
    writer.write_zeros(8u);
    return writer.status();
}

/* An answer that fits carries all of itself; one too large carries nothing
 * and says what it needed. */
bool ReplyHeader::fields_valid() const
{
    if (fence_id_ == 0u) return false;
    if (status_ == status_ok) return size_ == required_;
    return status_ == status_too_large && size_ == 0u && required_ != 0u;
}

Status ReplyHeader::decode(const uint8_t *bytes, size_t length)
{
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    ReplyHeader    candidate;
    candidate.status_   = reader.read_u32();
    candidate.fence_id_ = reader.read_u64();
    candidate.size_     = reader.read_u32();
    candidate.required_ = reader.read_u32();
    reader.expect_zero(8u);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
