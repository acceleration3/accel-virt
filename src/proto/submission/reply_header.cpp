#include "proto/submission/reply_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ReplyHeader::ReplyHeader() : status_(status_ok), fence_id_(0u), size_(0u), required_(0u)
{
    AV_SCOPE("ReplyHeader::ReplyHeader");
}

ReplyHeader::ReplyHeader(uint64_t fence_id, uint32_t size)
    : status_(status_ok), fence_id_(fence_id), size_(size), required_(size)
    {
        AV_SCOPE("ReplyHeader::ReplyHeader");
    }

ReplyHeader::ReplyHeader(uint32_t status, uint64_t fence_id, uint32_t size, uint32_t required)
    : status_(status), fence_id_(fence_id), size_(size), required_(required)
    {
        AV_SCOPE("ReplyHeader::ReplyHeader");
    }

ReplyHeader ReplyHeader::within(uint32_t capacity) const
{
    AV_SCOPE("ReplyHeader::within");
    return required_ <= capacity ? *this : ReplyHeader(status_too_large, fence_id_, 0u, required_);
}

bool     ReplyHeader::too_large() const
{
    AV_SCOPE("ReplyHeader::too_large");
    return status_ == status_too_large;
}
uint64_t ReplyHeader::fence_id() const
{
    AV_SCOPE("ReplyHeader::fence_id");
    return fence_id_;
}
uint32_t ReplyHeader::size() const
{
    AV_SCOPE("ReplyHeader::size");
    return size_;
}
uint32_t ReplyHeader::required() const
{
    AV_SCOPE("ReplyHeader::required");
    return required_;
}

Status ReplyHeader::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ReplyHeader::encode");
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

bool ReplyHeader::fields_valid() const
{
    AV_SCOPE("ReplyHeader::fields_valid");
    if (fence_id_ == 0u) return false;
    if (status_ == status_ok) return size_ == required_;
    return status_ == status_too_large && size_ == 0u && required_ != 0u;
}

Status ReplyHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ReplyHeader::decode");
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

}
