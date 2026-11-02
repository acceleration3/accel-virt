#include "proto/escape/present_point_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentPointRecord::PresentPointRecord() : point_(0u)
{
    AV_SCOPE("PresentPointRecord::PresentPointRecord");
}

PresentPointRecord::PresentPointRecord(uint32_t point) : point_(point)
{
    AV_SCOPE("PresentPointRecord::PresentPointRecord");
}

uint32_t PresentPointRecord::point() const
{
    AV_SCOPE("PresentPointRecord::point");
    return point_;
}

Status PresentPointRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("PresentPointRecord::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(point_);
    writer.write_u64(0u);
    return writer.status();
}

Status PresentPointRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("PresentPointRecord::decode");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t point = reader.read_u32();
    const uint64_t reserved = reader.read_u64();
    if (reader.status() != Status::ok) return Status::malformed;
    if (found_magic != magic || reserved != 0u || point == 0u) return Status::malformed;
    point_ = point;
    return Status::ok;
}

}
