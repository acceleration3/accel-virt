#include "proto/submission/submission_descriptor.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

SubmissionDescriptor::SubmissionDescriptor() : fence_id_(0u), begin_(0u), end_(0u)
{
    AV_SCOPE("SubmissionDescriptor::SubmissionDescriptor");
}

SubmissionDescriptor::SubmissionDescriptor(uint64_t fence_id, uint64_t begin, uint64_t end)
    : fence_id_(fence_id), begin_(begin), end_(end)
    {
        AV_SCOPE("SubmissionDescriptor::SubmissionDescriptor");
    }

uint64_t SubmissionDescriptor::fence_id() const
{
    AV_SCOPE("SubmissionDescriptor::fence_id");
    return fence_id_;
}
uint64_t SubmissionDescriptor::begin() const
{
    AV_SCOPE("SubmissionDescriptor::begin");
    return begin_;
}
uint64_t SubmissionDescriptor::end() const
{
    AV_SCOPE("SubmissionDescriptor::end");
    return end_;
}
uint64_t SubmissionDescriptor::length() const
{
    AV_SCOPE("SubmissionDescriptor::length");
    return end_ - begin_;
}

Status SubmissionDescriptor::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("SubmissionDescriptor::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_zeros(4u);
    writer.write_u64(fence_id_);
    writer.write_u64(begin_);
    writer.write_u64(end_);
    return writer.status();
}

Status SubmissionDescriptor::decode(const uint8_t *bytes, size_t length_bytes)
{
    AV_SCOPE("SubmissionDescriptor::decode");
    WireReader reader(bytes, length_bytes);
    const uint32_t found_magic = reader.read_u32();
    reader.expect_zero(4u);
    const uint64_t fence_id = reader.read_u64();
    const uint64_t begin = reader.read_u64();
    const uint64_t end = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || fence_id == 0u || end <= begin) return Status::malformed;
    *this = SubmissionDescriptor(fence_id, begin, end);
    return Status::ok;
}

Status SubmissionDescriptor::validate_against(const RingHeader &ring) const
{
    AV_SCOPE("SubmissionDescriptor::validate_against");
    return length() <= ring.capacity() ? Status::ok : Status::invalid_state;
}

}
