#include "proto/resources/host_visible_bind_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

HostVisibleBindRecord::HostVisibleBindRecord() : operation_(0u), allocation_(0u), resource_id_(0u), pages_(0u)
{
    AV_SCOPE("HostVisibleBindRecord::HostVisibleBindRecord");
}

HostVisibleBindRecord::HostVisibleBindRecord(Operation operation, uint32_t allocation, uint32_t resource_id)
    : operation_(static_cast<uint16_t>(operation)), allocation_(allocation), resource_id_(resource_id), pages_(0u)
{
    AV_SCOPE("HostVisibleBindRecord::HostVisibleBindRecord");
}

bool     HostVisibleBindRecord::binds() const
{
    AV_SCOPE("HostVisibleBindRecord::binds");
    return operation_ == static_cast<uint16_t>(Operation::bind);
}
uint32_t HostVisibleBindRecord::allocation() const
{
    AV_SCOPE("HostVisibleBindRecord::allocation");
    return allocation_;
}
uint32_t HostVisibleBindRecord::resource_id() const
{
    AV_SCOPE("HostVisibleBindRecord::resource_id");
    return resource_id_;
}
uint64_t HostVisibleBindRecord::pages() const
{
    AV_SCOPE("HostVisibleBindRecord::pages");
    return pages_;
}

HostVisibleBindRecord HostVisibleBindRecord::answered(uint32_t resource_id, uint64_t pages) const
{
    AV_SCOPE("HostVisibleBindRecord::answered");
    HostVisibleBindRecord answer = *this;
    answer.resource_id_ = resource_id;
    answer.pages_ = pages;
    return answer;
}

Status HostVisibleBindRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("HostVisibleBindRecord::encode");
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
    AV_SCOPE("HostVisibleBindRecord::is_request");
    if (pages_ != 0u) return false;
    if (operation_ == static_cast<uint16_t>(Operation::bind)) return allocation_ != 0u && resource_id_ == 0u;
    return operation_ == static_cast<uint16_t>(Operation::unbind) && resource_id_ != 0u;
}

Status HostVisibleBindRecord::read(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("HostVisibleBindRecord::read");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    operation_ = reader.read_u16();
    allocation_ = reader.read_u32();
    resource_id_ = reader.read_u32();
    pages_ = reader.read_u64();
    reader.expect_zero(8u);
    if (reader.status() != Status::ok) return reader.status();
    return found_magic == magic && found_version == version ? Status::ok : Status::malformed;
}

Status HostVisibleBindRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("HostVisibleBindRecord::decode");
    HostVisibleBindRecord candidate;
    const Status          read_back = candidate.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (!candidate.is_request()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

/* The kernel owns only the resource and the pages; every other byte must be
 * the request's own. */
Status HostVisibleBindRecord::accept_answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("HostVisibleBindRecord::accept_answer");
    HostVisibleBindRecord answer;
    const Status          read_back = answer.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (!is_request() || answer.operation_ != operation_ || answer.allocation_ != allocation_)
        return Status::malformed;
    const bool answered = binds() ? answer.resource_id_ != 0u && answer.pages_ != 0u
                                  : answer.resource_id_ == resource_id_ && answer.pages_ == 0u;
    if (!answered) return Status::malformed;
    *this = answer;
    return Status::ok;
}

}  // namespace av
