#include "proto/pages/vblank_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

VblankRecord::VblankRecord() : latched_(0u), sequence_(0u), vblank_ns_(0u), period_ns_(0u), refresh_millihz_(0u)
{
    AV_SCOPE("VblankRecord::VblankRecord");
}

VblankRecord::VblankRecord(uint32_t latched, uint64_t sequence, uint64_t vblank_ns, uint32_t refresh_millihz)
    : latched_(latched), sequence_(sequence), vblank_ns_(vblank_ns),
      period_ns_(refresh_millihz == 0u ? 0u : static_cast<uint32_t>(nanoseconds_times_millihz / refresh_millihz)),
      refresh_millihz_(refresh_millihz)
      {
          AV_SCOPE("VblankRecord::VblankRecord");
      }

uint32_t VblankRecord::latched() const
{
    AV_SCOPE("VblankRecord::latched");
    return latched_;
}

uint64_t VblankRecord::sequence() const
{
    AV_SCOPE("VblankRecord::sequence");
    return sequence_;
}

uint64_t VblankRecord::vblank_ns() const
{
    AV_SCOPE("VblankRecord::vblank_ns");
    return vblank_ns_;
}

uint32_t VblankRecord::period_ns() const
{
    AV_SCOPE("VblankRecord::period_ns");
    return period_ns_;
}

uint32_t VblankRecord::refresh_millihz() const
{
    AV_SCOPE("VblankRecord::refresh_millihz");
    return refresh_millihz_;
}

Status VblankRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("VblankRecord::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(latched_);
    writer.write_u64(sequence_);
    writer.write_u64(vblank_ns_);
    writer.write_u32(period_ns_);
    writer.write_u32(refresh_millihz_);
    return writer.status();
}

Status VblankRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("VblankRecord::decode");
    WireReader     reader(bytes, length);
    const uint32_t latched = reader.read_u32();
    const uint64_t sequence = reader.read_u64();
    const uint64_t vblank = reader.read_u64();
    const uint32_t period = reader.read_u32();
    const uint32_t refresh = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    if (refresh < least_millihz || refresh > most_millihz) return Status::malformed;
    if (period != nanoseconds_times_millihz / refresh) return Status::malformed;
    latched_ = latched;
    sequence_ = sequence;
    vblank_ns_ = vblank;
    period_ns_ = period;
    refresh_millihz_ = refresh;
    return Status::ok;
}

}
