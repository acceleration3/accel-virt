#include "proto/resources/host_visible_bind_record.hpp"

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

HostVisibleBindRecord::HostVisibleBindRecord() : operation_(0u), allocation_(0u), resource_id_(0u), pages_(0u) {}

HostVisibleBindRecord::HostVisibleBindRecord(Operation operation, uint32_t allocation, uint32_t resource_id)
    : operation_(static_cast<uint16_t>(operation)), allocation_(allocation), resource_id_(resource_id), pages_(0u)
{}

bool     HostVisibleBindRecord::binds() const { return operation_ == static_cast<uint16_t>(Operation::bind); }
uint32_t HostVisibleBindRecord::allocation() const { return allocation_; }
uint32_t HostVisibleBindRecord::resource_id() const { return resource_id_; }
uint64_t HostVisibleBindRecord::pages() const { return pages_; }

HostVisibleBindRecord HostVisibleBindRecord::answered(uint32_t resource_id, uint64_t pages) const
{
    HostVisibleBindRecord answer = *this;
    answer.resource_id_ = resource_id;
    answer.pages_ = pages;
    return answer;
}

Status HostVisibleBindRecord::encode(uint8_t *bytes, size_t capacity) const
{
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version);
    writer.write_u16(operation_);
    writer.write_u32(allocation_);
    writer.write_u32(resource_id_);
    writer.write_u64(pages_);
    writer.write_zeros(8u);
    return writer.status();
}

/* A bind names an allocation and leaves the answer's fields empty; an unbind
 * names a resource and nothing else the kernel answers. */
bool HostVisibleBindRecord::is_request() const
{
    if (pages_ != 0u) return false;
    if (operation_ == static_cast<uint16_t>(Operation::bind)) return allocation_ != 0u && resource_id_ == 0u;
    return operation_ == static_cast<uint16_t>(Operation::unbind) && resource_id_ != 0u;
}

Status HostVisibleBindRecord::decode(const uint8_t *bytes, size_t length)
{
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    HostVisibleBindRecord candidate;
    candidate.operation_ = reader.read_u16();
    candidate.allocation_ = reader.read_u32();
    candidate.resource_id_ = reader.read_u32();
    candidate.pages_ = reader.read_u64();
    reader.expect_zero(8u);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || found_version != version || !candidate.is_request()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
