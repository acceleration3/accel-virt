#include "proto/escape/host_window_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

constexpr uint64_t page_bytes = 4096u;

}

HostWindowRecord::HostWindowRecord() : operation_(0u), resource_id_(0u), memory_(0u), bytes_(0u), address_(0u)
{
    AV_SCOPE("HostWindowRecord::HostWindowRecord");
}

HostWindowRecord::HostWindowRecord(Operation operation, uint64_t bytes, uint32_t resource_id,
                                   HostMemoryRequest memory)
    : operation_(static_cast<uint16_t>(operation)), resource_id_(resource_id), memory_(memory.encoded()),
      bytes_(bytes), address_(0u)
{
    AV_SCOPE("HostWindowRecord::HostWindowRecord");
}

HostWindowRecord::HostWindowRecord(uint64_t address, uint64_t bytes, HostMemoryRequest memory)
    : operation_(static_cast<uint16_t>(Operation::adopt)), resource_id_(0u), memory_(memory.encoded()), bytes_(bytes),
      address_(address)
{
    AV_SCOPE("HostWindowRecord::HostWindowRecord");
}

bool HostWindowRecord::adopts() const
{
    AV_SCOPE("HostWindowRecord::adopts");
    return operation_ == static_cast<uint16_t>(Operation::adopt);
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

HostMemoryRequest HostWindowRecord::memory() const
{
    AV_SCOPE("HostWindowRecord::memory");
    return HostMemoryRequest(memory_);
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
    writer.write_u32(memory_);
    writer.write_u64(bytes_);
    writer.write_u64(address_);
    return writer.status();
}

bool HostWindowRecord::is_request() const
{
    AV_SCOPE("HostWindowRecord::is_request");
    if (adopts())
        return address_ != 0u && address_ % page_bytes == 0u && bytes_ != 0u && bytes_ % page_bytes == 0u &&
               resource_id_ == 0u && memory().valid();
    if (address_ != 0u) return false;
    if (operation_ == static_cast<uint16_t>(Operation::open))
        return bytes_ != 0u && resource_id_ == 0u && memory().valid();
    return operation_ == static_cast<uint16_t>(Operation::close) && resource_id_ != 0u && bytes_ == 0u &&
           memory_ == 0u;
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
    memory_ = reader.read_u32();
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

Status HostWindowRecord::accept_answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("HostWindowRecord::accept_answer");
    HostWindowRecord answer;
    const Status     read_back = answer.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (!is_request() || answer.operation_ != operation_ || answer.memory_ != memory_) return Status::malformed;
    bool answered = answer.resource_id_ == resource_id_ && answer.bytes_ == 0u && answer.address_ == 0u;
    if (opens()) answered = answer.resource_id_ != 0u && answer.bytes_ >= bytes_ && answer.address_ != 0u;
    if (adopts())
        answered = answer.resource_id_ != 0u && answer.bytes_ == bytes_ && answer.address_ == address_;
    if (!answered) return Status::malformed;
    *this = answer;
    return Status::ok;
}

}
