#include "proto/submission/batch_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BatchHeader::BatchHeader() : length_(0u), fence_id_(0u), published_at_(0u)
{
    AV_SCOPE("BatchHeader::BatchHeader");
}

BatchHeader::BatchHeader(uint32_t length, uint64_t fence_id) : BatchHeader(length, fence_id, 0u)
{
    AV_SCOPE("BatchHeader::BatchHeader");
}

BatchHeader::BatchHeader(uint32_t length, uint64_t fence_id, uint64_t published_at)
    : length_(length), fence_id_(fence_id), published_at_(published_at)
{
    AV_SCOPE("BatchHeader::BatchHeader");
}

uint64_t BatchHeader::published_at() const
{
    AV_SCOPE("BatchHeader::published_at");
    return published_at_;
}

uint32_t BatchHeader::length() const
{
    AV_SCOPE("BatchHeader::length");
    return length_;
}
uint64_t BatchHeader::fence_id() const
{
    AV_SCOPE("BatchHeader::fence_id");
    return fence_id_;
}

/* A u32 payload plus the header cannot wrap a u64. */
uint64_t BatchHeader::span() const
{
    AV_SCOPE("BatchHeader::span");
    return (encoded_size + uint64_t{length_} + alignment - 1u) & ~(alignment - 1u);
}

bool BatchHeader::fits(uint64_t capacity) const
{
    AV_SCOPE("BatchHeader::fits");
    return span() <= capacity;
}

Status BatchHeader::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("BatchHeader::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(length_);
    writer.write_u64(fence_id_);
    writer.write_u64(published_at_);
    writer.write_zeros(8u);
    return writer.status();
}

Status BatchHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("BatchHeader::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t payload = reader.read_u32();
    const uint64_t fence_id = reader.read_u64();
    const uint64_t published_at = reader.read_u64();
    reader.expect_zero(8u);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || fence_id == 0u) return Status::malformed;
    *this = BatchHeader(payload, fence_id, published_at);
    return Status::ok;
}

}  // namespace av
