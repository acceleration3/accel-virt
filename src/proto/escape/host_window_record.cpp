#include "proto/escape/host_window_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

HostWindowRecord::HostWindowRecord() : operation_(0u), resource_id_(0u), bytes_(0u), address_(0u)
{
    AV_SCOPE("HostWindowRecord::HostWindowRecord");
}

HostWindowRecord::HostWindowRecord(Operation operation, uint64_t bytes, uint32_t resource_id)
    : operation_(static_cast<uint16_t>(operation)), resource_id_(resource_id), bytes_(bytes), address_(0u)
{
    AV_SCOPE("HostWindowRecord::HostWindowRecord");
}

bool HostWindowRecord::opens() const
{
    AV_SCOPE("HostWindowRecord::opens");
    return operation_ == static_cast<uint16_t>(Operation::open);
}

uint32_t HostWindowRecord::resource_id() const
{
    AV_SCOPE("HostWindowRecord::resource_id");
    return resource_id_;
}

uint64_t HostWindowRecord::bytes() const
{
    AV_SCOPE("HostWindowRecord::bytes");
    return bytes_;
}

uint64_t HostWindowRecord::address() const
{
    AV_SCOPE("HostWindowRecord::address");
    return address_;
}

HostWindowRecord HostWindowRecord::answered(uint32_t resource_id, uint64_t bytes, uint64_t address) const
{
    AV_SCOPE("HostWindowRecord::answered");
    HostWindowRecord answer = *this;
    answer.resource_id_ = resource_id;
    answer.bytes_ = bytes;
    answer.address_ = address;
    return answer;
}

Status HostWindowRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("HostWindowRecord::encode");
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version);
    writer.write_u16(operation_);
    writer.write_u32(resource_id_);
    writer.write_u32(0u);
    writer.write_u64(bytes_);
    writer.write_u64(address_);
    return writer.status();
}

/* An open names the bytes and leaves the answer's fields empty; a close names
 * a resource and nothing else. */
bool HostWindowRecord::is_request() const
{
    AV_SCOPE("HostWindowRecord::is_request");
    if (address_ != 0u) return false;
    if (operation_ == static_cast<uint16_t>(Operation::open)) return bytes_ != 0u && resource_id_ == 0u;
    return operation_ == static_cast<uint16_t>(Operation::close) && resource_id_ != 0u && bytes_ == 0u;
}

Status HostWindowRecord::read(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("HostWindowRecord::read");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    operation_ = reader.read_u16();
    resource_id_ = reader.read_u32();
    reader.expect_zero(4u);
    bytes_ = reader.read_u64();
    address_ = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    return found_magic == magic && found_version == version ? Status::ok : Status::malformed;
}

Status HostWindowRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("HostWindowRecord::decode");
    HostWindowRecord candidate;
    const Status     read_back = candidate.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (!candidate.is_request()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

/* The kernel owns the resource, the granted size and the address; the
 * operation must be the request's own. */
Status HostWindowRecord::accept_answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("HostWindowRecord::accept_answer");
    HostWindowRecord answer;
    const Status     read_back = answer.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (!is_request() || answer.operation_ != operation_) return Status::malformed;
    const bool answered = opens() ? answer.resource_id_ != 0u && answer.bytes_ >= bytes_ && answer.address_ != 0u
                                  : answer.resource_id_ == resource_id_ && answer.bytes_ == 0u && answer.address_ == 0u;
    if (!answered) return Status::malformed;
    *this = answer;
    return Status::ok;
}

}  // namespace av
