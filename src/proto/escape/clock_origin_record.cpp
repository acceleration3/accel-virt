#include "proto/escape/clock_origin_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ClockOriginRecord::ClockOriginRecord() : counter_(0u), ticks_(0u), frequency_(0u)
{
    AV_SCOPE("ClockOriginRecord::ClockOriginRecord");
}

ClockOriginRecord::ClockOriginRecord(uint64_t counter, uint64_t ticks, uint64_t frequency)
    : counter_(counter), ticks_(ticks), frequency_(frequency)
{
    AV_SCOPE("ClockOriginRecord::ClockOriginRecord");
}

uint64_t ClockOriginRecord::counter() const
{
    AV_SCOPE("ClockOriginRecord::counter");
    return counter_;
}

uint64_t ClockOriginRecord::ticks() const
{
    AV_SCOPE("ClockOriginRecord::ticks");
    return ticks_;
}

uint64_t ClockOriginRecord::frequency() const
{
    AV_SCOPE("ClockOriginRecord::frequency");
    return frequency_;
}

Status ClockOriginRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ClockOriginRecord::encode");
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version);
    writer.write_zeros(2u);
    writer.write_u64(counter_);
    writer.write_u64(ticks_);
    writer.write_u64(frequency_);
    return writer.status();
}

Status ClockOriginRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ClockOriginRecord::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    reader.expect_zero(2u);
    const uint64_t counter = reader.read_u64();
    const uint64_t ticks = reader.read_u64();
    const uint64_t frequency = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || found_version != version || counter == 0u || frequency == 0u ||
        frequency > most_frequency)
        return Status::malformed;
    counter_ = counter;
    ticks_ = ticks;
    frequency_ = frequency;
    return Status::ok;
}

}
