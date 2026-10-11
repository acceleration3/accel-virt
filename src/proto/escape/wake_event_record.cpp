#include "proto/escape/wake_event_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

WakeEventRecord::WakeEventRecord() : event_(0u)
{
    AV_SCOPE("WakeEventRecord::WakeEventRecord");
}

WakeEventRecord::WakeEventRecord(uint64_t event) : event_(event)
{
    AV_SCOPE("WakeEventRecord::WakeEventRecord");
}

uint64_t WakeEventRecord::event() const
{
    AV_SCOPE("WakeEventRecord::event");
    return event_;
}

Status WakeEventRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("WakeEventRecord::encode");
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version);
    writer.write_zeros(2u);
    writer.write_u64(event_);
    writer.write_zeros(16u);
    return writer.status();
}

Status WakeEventRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("WakeEventRecord::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    reader.expect_zero(2u);
    const uint64_t event = reader.read_u64();
    reader.expect_zero(16u);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || found_version != version || event == 0u) return Status::malformed;
    event_ = event;
    return Status::ok;
}

}  // namespace av
