#include "proto/escape/follow_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

FollowRecord::FollowRecord() : ring_(0u), fence_(0u)
{
    AV_SCOPE("FollowRecord::FollowRecord");
}

FollowRecord::FollowRecord(uint32_t ring, uint64_t fence) : ring_(ring), fence_(fence)
{
    AV_SCOPE("FollowRecord::FollowRecord");
}

uint32_t FollowRecord::ring() const
{
    AV_SCOPE("FollowRecord::ring");
    return ring_;
}

uint64_t FollowRecord::fence() const
{
    AV_SCOPE("FollowRecord::fence");
    return fence_;
}

Status FollowRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("FollowRecord::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(ring_);
    writer.write_u64(fence_);
    return writer.status();
}

Status FollowRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("FollowRecord::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t ring = reader.read_u32();
    const uint64_t fence = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || ring == 0u || fence == 0u) return Status::malformed;
    *this = FollowRecord(ring, fence);
    return Status::ok;
}

}  // namespace av
